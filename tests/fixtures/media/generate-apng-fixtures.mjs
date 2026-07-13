import {createHash} from "node:crypto";
import {mkdirSync, writeFileSync} from "node:fs";
import {deflateSync} from "node:zlib";

const outputDirectory = new URL("apng/", import.meta.url);
mkdirSync(outputDirectory, {recursive: true});

const signature = Buffer.from("89504e470d0a1a0a", "hex");

const crcTable = Array.from({length: 256}, (_, value) => {
    let crc = value;
    for (let bit = 0; bit < 8; ++bit) {
        crc = (crc & 1) !== 0 ? 0xedb88320 ^ (crc >>> 1) : crc >>> 1;
    }
    return crc >>> 0;
});

function crc32(data) {
    let crc = 0xffffffff;
    for (const byte of data) {
        crc = crcTable[(crc ^ byte) & 0xff] ^ (crc >>> 8);
    }
    return (crc ^ 0xffffffff) >>> 0;
}

function u32(value) {
    const result = Buffer.alloc(4);
    result.writeUInt32BE(value);
    return result;
}

function u16(value) {
    const result = Buffer.alloc(2);
    result.writeUInt16BE(value);
    return result;
}

function chunk(type, data = Buffer.alloc(0)) {
    const typeBytes = Buffer.from(type, "ascii");
    return Buffer.concat([u32(data.length), typeBytes, data, u32(crc32(Buffer.concat([typeBytes, data])))]);
}

function ihdr(width, height, colorType) {
    return chunk("IHDR", Buffer.concat([
        u32(width),
        u32(height),
        Buffer.from([8, colorType, 0, 0, 0])
    ]));
}

function fctl(sequence, frame) {
    return chunk("fcTL", Buffer.concat([
        u32(sequence),
        u32(frame.width),
        u32(frame.height),
        u32(frame.x),
        u32(frame.y),
        u16(frame.delayNumerator),
        u16(frame.delayDenominator),
        Buffer.from([frame.dispose, frame.blend])
    ]));
}

function compressedRows(frame, bytesPerPixel) {
    const rowBytes = frame.width * bytesPerPixel;
    const raw = Buffer.alloc(frame.height * (rowBytes + 1));
    for (let y = 0; y < frame.height; ++y) {
        frame.pixels.copy(raw, y * (rowBytes + 1) + 1, y * rowBytes, (y + 1) * rowBytes);
    }
    return deflateSync(raw, {level: 9});
}

function buildApng({width, height, frames, plays = 0, palette = null, beforeAnimationControl = []}) {
    const colorType = palette === null ? 6 : 3;
    const chunks = [signature, ihdr(width, height, colorType), ...beforeAnimationControl];
    chunks.push(chunk("acTL", Buffer.concat([u32(frames.length), u32(plays)])));
    if (palette !== null) {
        chunks.push(chunk("PLTE", Buffer.from(palette.colors.flat())));
        if (palette.alpha !== undefined) {
            chunks.push(chunk("tRNS", Buffer.from(palette.alpha)));
        }
    }

    let sequence = 0;
    for (let index = 0; index < frames.length; ++index) {
        const frame = frames[index];
        chunks.push(fctl(sequence++, frame));
        const compressed = compressedRows(frame, colorType === 6 ? 4 : 1);
        if (index === 0) {
            chunks.push(chunk("IDAT", compressed));
        } else {
            chunks.push(chunk("fdAT", Buffer.concat([u32(sequence++), compressed])));
        }
    }
    chunks.push(chunk("IEND"));
    return Buffer.concat(chunks);
}

function solid(width, height, rgba) {
    const result = Buffer.alloc(width * height * 4);
    for (let offset = 0; offset < result.length; offset += 4) {
        result.set(rgba, offset);
    }
    return result;
}

function pixels(width, height, values) {
    if (values.length !== width * height) {
        throw new Error("pixel matrix does not match its dimensions");
    }
    return Buffer.from(values.flat());
}

function sha256(data) {
    return createHash("sha256").update(data).digest("hex");
}

function clearRectangle(canvas, canvasWidth, frame) {
    for (let y = 0; y < frame.height; ++y) {
        const start = ((frame.y + y) * canvasWidth + frame.x) * 4;
        canvas.fill(0, start, start + frame.width * 4);
    }
}

function sourceCopy(canvas, canvasWidth, frame) {
    for (let y = 0; y < frame.height; ++y) {
        const sourceStart = y * frame.width * 4;
        const destinationStart = ((frame.y + y) * canvasWidth + frame.x) * 4;
        frame.pixels.copy(canvas, destinationStart, sourceStart, sourceStart + frame.width * 4);
    }
}

function sourceOverPixel(destination, destinationOffset, source, sourceOffset) {
    const sourceAlpha = source[sourceOffset + 3];
    const destinationAlpha = destination[destinationOffset + 3];
    if (sourceAlpha === 0) {
        return;
    }
    if (sourceAlpha === 255 || destinationAlpha === 0) {
        source.copy(destination, destinationOffset, sourceOffset, sourceOffset + 4);
        return;
    }

    const inverseSourceAlpha = 255 - sourceAlpha;
    const alphaNumerator = sourceAlpha * 255 + destinationAlpha * inverseSourceAlpha;
    for (let channel = 0; channel < 3; ++channel) {
        const colorNumerator = source[sourceOffset + channel] * sourceAlpha * 255
            + destination[destinationOffset + channel] * destinationAlpha * inverseSourceAlpha;
        destination[destinationOffset + channel] = Math.round(colorNumerator / alphaNumerator);
    }
    destination[destinationOffset + 3] = Math.round(alphaNumerator / 255);
}

function sourceOver(canvas, canvasWidth, frame) {
    for (let y = 0; y < frame.height; ++y) {
        for (let x = 0; x < frame.width; ++x) {
            const sourceOffset = (y * frame.width + x) * 4;
            const destinationOffset = ((frame.y + y) * canvasWidth + frame.x + x) * 4;
            sourceOverPixel(canvas, destinationOffset, frame.pixels, sourceOffset);
        }
    }
}

function timestamp(frame, callbackNumber) {
    if (callbackNumber === 0 || frame.delayDenominator === 0) {
        return null;
    }
    const seconds = Math.fround(frame.delayNumerator / frame.delayDenominator);
    const callbackSeconds = Math.fround(Math.fround(callbackNumber + 1) * seconds);
    return Math.trunc(Math.fround(callbackSeconds * 1000));
}

function validateChunkStream(data) {
    if (data.length < signature.length || !data.subarray(0, signature.length).equals(signature)) {
        throw new Error("invalid PNG signature");
    }
    let offset = signature.length;
    let expectedSequence = 0;
    let sawEnd = false;
    while (offset < data.length) {
        if (data.length - offset < 12) {
            throw new Error("truncated chunk header");
        }
        const length = data.readUInt32BE(offset);
        const end = offset + 12 + length;
        if (!Number.isSafeInteger(end) || end > data.length) {
            throw new Error("truncated chunk data");
        }
        const typeBytes = data.subarray(offset + 4, offset + 8);
        const type = typeBytes.toString("ascii");
        const contents = data.subarray(offset + 8, offset + 8 + length);
        const storedCrc = data.readUInt32BE(offset + 8 + length);
        const calculatedCrc = crc32(Buffer.concat([typeBytes, contents]));
        if (storedCrc !== calculatedCrc) {
            throw new Error(`CRC mismatch in ${type}`);
        }
        if (type === "fcTL" || type === "fdAT") {
            if (contents.length < 4 || contents.readUInt32BE(0) !== expectedSequence++) {
                throw new Error(`invalid APNG sequence in ${type}`);
            }
        }
        offset = end;
        if (type === "IEND") {
            sawEnd = true;
            break;
        }
    }
    if (!sawEnd || offset !== data.length) {
        throw new Error("missing or non-terminal IEND");
    }
}

function expectedFrames(fixture) {
    if (fixture.palette !== null && fixture.palette !== undefined) {
        return [];
    }
    const canvas = Buffer.from(fixture.frames[0].pixels);
    const result = [];
    for (let callbackNumber = 1; callbackNumber < fixture.frames.length; ++callbackNumber) {
        const frame = fixture.frames[callbackNumber];
        const previous = Buffer.from(canvas);
        if (frame.blend === 0) {
            clearRectangle(canvas, fixture.width, frame);
            sourceCopy(canvas, fixture.width, frame);
        } else if (frame.blend === 1) {
            sourceOver(canvas, fixture.width, frame);
        } else {
            return [];
        }

        const displayedHash = sha256(canvas);
        if (frame.dispose === 1) {
            clearRectangle(canvas, fixture.width, frame);
        } else if (frame.dispose === 2) {
            previous.copy(canvas);
        } else if (frame.dispose !== 0) {
            return [];
        }
        result.push({
            callbackNumber,
            timestampMs: timestamp(frame, callbackNumber),
            displayedRgbaSha256: displayedHash,
            nextCanvasRgbaSha256: sha256(canvas)
        });
    }
    return result;
}

const blue = [0, 0, 255, 255];
const red = [255, 0, 0, 255];
const halfRed = [255, 0, 0, 128];
const green = [0, 255, 0, 255];
const halfGreen = [0, 255, 0, 128];
const transparent = [0, 0, 0, 0];

function baseFrame(width = 4, height = 4) {
    return {
        width,
        height,
        x: 0,
        y: 0,
        delayNumerator: 1,
        delayDenominator: 10,
        dispose: 2,
        blend: 0,
        pixels: solid(width, height, blue)
    };
}

function frame(width, height, x, y, rgba, dispose, blend, delayNumerator = 1, delayDenominator = 10) {
    return {
        width,
        height,
        x,
        y,
        delayNumerator,
        delayDenominator,
        dispose,
        blend,
        pixels: solid(width, height, rgba)
    };
}

const fixtures = [];
for (const [disposeName, dispose] of [["none", 0], ["background", 1], ["previous", 2]]) {
    fixtures.push({
        id: `apng.composition-matrix.over-${disposeName}`,
        file: `over-${disposeName}.png`,
        width: 4,
        height: 4,
        palette: null,
        frames: [
            baseFrame(),
            frame(2, 2, 1, 1, halfRed, dispose, 1),
            frame(1, 1, 0, 0, green, 0, 0)
        ]
    });
    fixtures.push({
        id: `apng.composition-matrix.source-${disposeName}`,
        file: `source-${disposeName}.png`,
        width: 4,
        height: 4,
        palette: null,
        frames: [
            baseFrame(),
            {
                ...frame(2, 2, 1, 1, transparent, dispose, 0),
                pixels: pixels(2, 2, [red, transparent, transparent, halfRed])
            },
            frame(1, 1, 0, 0, green, 0, 0)
        ]
    });
}

fixtures.push({
    id: "apng.composition-matrix.transparent-history",
    file: "transparent-history.png",
    width: 4,
    height: 4,
    palette: null,
    frames: [
        baseFrame(),
        frame(2, 2, 0, 0, red, 0, 0),
        {
            ...frame(3, 2, 0, 0, transparent, 0, 1),
            pixels: pixels(3, 2, [transparent, green, transparent, green, transparent, transparent])
        }
    ]
});

fixtures.push({
    id: "apng.composition-matrix.source-after-over",
    file: "source-after-over.png",
    width: 4,
    height: 4,
    palette: null,
    frames: [baseFrame(), frame(2, 2, 1, 1, halfRed, 0, 1), frame(2, 1, 1, 1, green, 0, 0)]
});

fixtures.push({
    id: "apng.composition-matrix.over-after-source",
    file: "over-after-source.png",
    width: 4,
    height: 4,
    palette: null,
    frames: [baseFrame(), frame(2, 2, 1, 1, red, 0, 0), frame(2, 1, 1, 1, halfGreen, 0, 1)]
});

fixtures.push({
    id: "apng.canvas-transitions.canvas-edges",
    file: "canvas-edges.png",
    width: 4,
    height: 4,
    palette: null,
    frames: [
        baseFrame(),
        frame(4, 1, 0, 0, red, 0, 0),
        frame(1, 4, 3, 0, green, 0, 1),
        frame(4, 1, 0, 3, halfRed, 0, 1),
        frame(1, 4, 0, 0, red, 0, 0)
    ]
});

fixtures.push({
    id: "apng.animation-options-and-timing.varying-delays",
    file: "varying-delays-loop.png",
    width: 4,
    height: 4,
    plays: 7,
    palette: null,
    frames: [
        baseFrame(),
        frame(1, 1, 0, 0, red, 0, 0, 1, 10),
        frame(1, 1, 1, 0, green, 0, 0, 1, 4),
        frame(1, 1, 2, 0, halfRed, 0, 1, 3, 10)
    ]
});

fixtures.push({
    id: "apng.first-callback.omitted-and-disposal-ignored",
    file: "first-callback-omitted.png",
    width: 4,
    height: 4,
    palette: null,
    frames: [baseFrame(), frame(4, 4, 0, 0, transparent, 0, 1)]
});

fixtures.push({
    id: "apng.composition-matrix.opaque-over",
    file: "opaque-over.png",
    width: 4,
    height: 4,
    palette: null,
    frames: [baseFrame(), frame(2, 2, 0, 0, red, 0, 1)]
});

fixtures.push({
    id: "apng.fixed-offset-detection.other-offset",
    file: "animation-control-other-offset.png",
    width: 4,
    height: 4,
    palette: null,
    beforeAnimationControl: [chunk("tEXt", Buffer.from("fixture=offset", "ascii"))],
    frames: [baseFrame(), frame(1, 1, 0, 0, red, 0, 0)]
});

fixtures.push({
    id: "apng.branching.palette-static",
    file: "palette-static.png",
    width: 2,
    height: 2,
    palette: {
        colors: [[0, 0, 255], [255, 0, 0], [0, 255, 0]],
        alpha: [255, 255, 128]
    },
    frames: [
        {...baseFrame(2, 2), pixels: Buffer.from([0, 0, 0, 0])},
        {...frame(2, 2, 0, 0, red, 0, 0), pixels: Buffer.from([1, 2, 2, 1])}
    ]
});

const malformedFixtures = [
    {
        id: "apng.chunk-validation.out-of-bounds-right",
        file: "invalid-out-of-bounds-right.png",
        expectedError: "frame-rectangle",
        data: buildApng({width: 4, height: 4, frames: [baseFrame(), frame(2, 1, 3, 0, red, 0, 0)]})
    },
    {
        id: "apng.chunk-validation.out-of-bounds-bottom",
        file: "invalid-out-of-bounds-bottom.png",
        expectedError: "frame-rectangle",
        data: buildApng({width: 4, height: 4, frames: [baseFrame(), frame(1, 2, 0, 3, red, 0, 0)]})
    },
    {
        id: "apng.chunk-validation.zero-denominator",
        file: "invalid-zero-denominator.png",
        expectedError: "delay-denominator",
        data: buildApng({width: 4, height: 4, frames: [baseFrame(), frame(1, 1, 0, 0, red, 0, 0, 1, 0)]})
    },
    {
        id: "apng.chunk-validation.unknown-blend",
        file: "invalid-unknown-blend.png",
        expectedError: "blend-operation",
        data: buildApng({width: 4, height: 4, frames: [baseFrame(), frame(1, 1, 0, 0, red, 0, 2)]})
    },
    {
        id: "apng.chunk-validation.unknown-dispose",
        file: "invalid-unknown-dispose.png",
        expectedError: "dispose-operation",
        data: buildApng({width: 4, height: 4, frames: [baseFrame(), frame(1, 1, 0, 0, red, 3, 0)]})
    }
];

const crcSource = buildApng({width: 4, height: 4, frames: [baseFrame(), frame(1, 1, 0, 0, red, 0, 0)]});
const frameControlTypeOffset = crcSource.indexOf(Buffer.from("fcTL", "ascii"));
const crcMismatch = Buffer.from(crcSource);
crcMismatch[frameControlTypeOffset + 8] ^= 1;
malformedFixtures.push({
    id: "apng.chunk-validation.crc-mismatch",
    file: "invalid-crc.png",
    expectedError: "crc",
    data: crcMismatch
});
malformedFixtures.push({
    id: "apng.chunk-validation.truncated",
    file: "invalid-truncated.png",
    expectedError: "truncated-chunk",
    data: crcSource.subarray(0, crcSource.length - 7)
});
malformedFixtures.push({
    id: "apng.chunk-validation.length-overflow",
    file: "invalid-length-overflow.png",
    expectedError: "chunk-length",
    data: Buffer.concat([signature, u32(0xffffffff), Buffer.from("acTL", "ascii")])
});
malformedFixtures.push({
    id: "apng.fixed-offset-detection.length-41",
    file: "detection-length-41.png",
    expectedError: "not-apng-length",
    data: crcSource.subarray(0, 41)
});

const manifest = {
    schemaVersion: 1,
    license: "MIT",
    provenance: "Generated from source pixel matrices in generate-apng-fixtures.mjs; no external image or historical implementation is used",
    fixtures: []
};

for (const fixture of fixtures) {
    const data = buildApng(fixture);
    validateChunkStream(data);
    writeFileSync(new URL(fixture.file, outputDirectory), data);
    const fixedOffsetApng = data.length > 41 && data.subarray(37, 41).equals(Buffer.from("acTL"));
    const fixedOffsetPalette = data.length > 64 && data.subarray(57, 61).equals(Buffer.from("PLTE"));
    manifest.fixtures.push({
        id: fixture.id,
        file: fixture.file,
        inputSha256: sha256(data),
        canvas: {width: fixture.width, height: fixture.height},
        fixedOffsetApng,
        fixedOffsetPalette,
        expectedClassification: !fixedOffsetApng ? "not-apng" : fixedOffsetPalette ? "apng-palette" : "apng-nonpalette",
        inputLoopCount: fixture.plays ?? 0,
        emittedFrames: fixedOffsetApng && !fixedOffsetPalette ? expectedFrames(fixture) : []
    });
}

for (const fixture of malformedFixtures) {
    if (["crc", "truncated-chunk", "chunk-length", "not-apng-length"].includes(fixture.expectedError)) {
        let rejected = false;
        try {
            validateChunkStream(fixture.data);
        } catch {
            rejected = true;
        }
        if (!rejected) {
            throw new Error(`${fixture.file} unexpectedly has a valid chunk stream`);
        }
    } else {
        validateChunkStream(fixture.data);
    }
    writeFileSync(new URL(fixture.file, outputDirectory), fixture.data);
    manifest.fixtures.push({
        id: fixture.id,
        file: fixture.file,
        inputSha256: sha256(fixture.data),
        fixedOffsetApng: fixture.data.length > 41 && fixture.data.subarray(37, 41).equals(Buffer.from("acTL")),
        expectedError: fixture.expectedError
    });
}

writeFileSync(new URL("manifest.json", outputDirectory), `${JSON.stringify(manifest, null, 4)}\n`);
