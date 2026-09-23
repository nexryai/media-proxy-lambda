use std::panic::{AssertUnwindSafe, catch_unwind};
use std::ptr;
use std::slice;
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};

use resvg::{tiny_skia, usvg};

const MAX_DATA_URLS: usize = 128;
const MAX_DATA_URL_BYTES: usize = 10 * 1024 * 1024;
const MAX_IMAGE_WIDTH: usize = 7680;
const MAX_IMAGE_HEIGHT: usize = 4320;
const MAX_IMAGE_PIXELS: usize = MAX_IMAGE_WIDTH * MAX_IMAGE_HEIGHT;
const MAX_XML_NODES: usize = 100_000;
const DEFAULT_FONT_FAMILY: &str = "M PLUS 1p";

#[repr(C)]
#[derive(Clone, Copy)]
pub struct MpResvgSize {
    pub width: u32,
    pub height: u32,
}

#[repr(i32)]
#[derive(Clone, Copy)]
enum Status {
    Ok = 0,
    InvalidArgument = 1,
    InvalidDocument = 2,
    InvalidSize = 3,
    RenderFailed = 4,
    Panic = 5,
}

pub struct MpResvgTree {
    tree: usvg::Tree,
    size: tiny_skia::IntSize,
}

fn contains_doctype(data: &[u8]) -> bool {
    data.windows(b"<!DOCTYPE".len())
        .any(|window| window == b"<!DOCTYPE")
}

fn validate_xml(data: &[u8]) -> Result<(), Status> {
    if data.is_empty() || contains_doctype(data) {
        return Err(Status::InvalidDocument);
    }
    let xml = std::str::from_utf8(data)
        .map_err(|_| Status::InvalidDocument)?;
    let document = roxmltree::Document::parse(xml)
        .map_err(|_| Status::InvalidDocument)?;
    if document.descendants().take(MAX_XML_NODES + 1).count()
        > MAX_XML_NODES
    {
        return Err(Status::InvalidDocument);
    }
    Ok(())
}

fn parse_impl(
    data: &[u8],
    font: &[u8],
) -> Result<Box<MpResvgTree>, Status> {
    if font.is_empty() {
        return Err(Status::InvalidDocument);
    }
    validate_xml(data)?;

    let mut options = usvg::Options::default();
    options.font_family = DEFAULT_FONT_FAMILY.to_owned();
    options.fontdb_mut().load_font_data(font.to_vec());
    if options.fontdb.faces().next().is_none() {
        return Err(Status::InvalidArgument);
    }
    {
        let database = options.fontdb_mut();
        database.set_serif_family(DEFAULT_FONT_FAMILY);
        database.set_sans_serif_family(DEFAULT_FONT_FAMILY);
        database.set_cursive_family(DEFAULT_FONT_FAMILY);
        database.set_fantasy_family(DEFAULT_FONT_FAMILY);
        database.set_monospace_family(DEFAULT_FONT_FAMILY);
    }

    let data_url_count = Arc::new(AtomicUsize::new(0));
    let data_url_bytes = Arc::new(AtomicUsize::new(0));
    let default_data_resolver = usvg::ImageHrefResolver::default_data_resolver();
    options.image_href_resolver = usvg::ImageHrefResolver {
        resolve_data: Box::new(move |mime, decoded, nested_options| {
            let previous_count = data_url_count.fetch_add(1, Ordering::Relaxed);
            if previous_count >= MAX_DATA_URLS || contains_doctype(&decoded) {
                return None;
            }
            let byte_budget = data_url_bytes.fetch_update(
                Ordering::Relaxed,
                Ordering::Relaxed,
                |value| {
                    value.checked_add(decoded.len())
                        .filter(|next| *next <= MAX_DATA_URL_BYTES)
                },
            );
            if byte_budget.is_err() {
                return None;
            }
            let raster_size = imagesize::blob_size(&decoded);
            if let Ok(size) = &raster_size {
                if size.width == 0
                    || size.height == 0
                    || size.width > MAX_IMAGE_WIDTH
                    || size.height > MAX_IMAGE_HEIGHT
                    || size.width.checked_mul(size.height)
                        .is_none_or(|pixels| pixels > MAX_IMAGE_PIXELS)
                {
                    return None;
                }
            }
            if (mime == "image/svg+xml"
                || (mime == "text/plain" && raster_size.is_err()))
                && validate_xml(&decoded).is_err()
            {
                return None;
            }
            default_data_resolver(mime, decoded, nested_options)
        }),
        resolve_string: Box::new(|_, _| None),
    };

    let tree = usvg::Tree::from_data(data, &options)
        .map_err(|_| Status::InvalidDocument)?;
    let size = tree.size().to_int_size();
    if size.width() == 0 || size.height() == 0 {
        return Err(Status::InvalidSize);
    }
    Ok(Box::new(MpResvgTree { tree, size }))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn mp_resvg_parse(
    data: *const u8,
    data_len: usize,
    font: *const u8,
    font_len: usize,
    output_tree: *mut *mut MpResvgTree,
    output_size: *mut MpResvgSize,
) -> i32 {
    if data.is_null()
        || font.is_null()
        || output_tree.is_null()
        || output_size.is_null()
        || data_len > isize::MAX as usize
        || font_len > isize::MAX as usize
    {
        return Status::InvalidArgument as i32;
    }
    unsafe {
        output_tree.write(ptr::null_mut());
        output_size.write(MpResvgSize { width: 0, height: 0 });
    }

    let result = catch_unwind(AssertUnwindSafe(|| {
        let data = unsafe { slice::from_raw_parts(data, data_len) };
        let font = unsafe { slice::from_raw_parts(font, font_len) };
        parse_impl(data, font)
    }));
    match result {
        Ok(Ok(tree)) => {
            let size = MpResvgSize {
                width: tree.size.width(),
                height: tree.size.height(),
            };
            unsafe {
                output_size.write(size);
                output_tree.write(Box::into_raw(tree));
            }
            Status::Ok as i32
        }
        Ok(Err(status)) => status as i32,
        Err(_) => Status::Panic as i32,
    }
}

fn render_impl(
    tree: &MpResvgTree,
    width: u32,
    height: u32,
    pixels: &mut [u8],
) -> Result<(), Status> {
    if width != tree.size.width() || height != tree.size.height() {
        return Err(Status::InvalidSize);
    }
    let expected = usize::try_from(width)
        .ok()
        .and_then(|value| value.checked_mul(usize::try_from(height).ok()?))
        .and_then(|value| value.checked_mul(4))
        .ok_or(Status::InvalidSize)?;
    if pixels.len() != expected {
        return Err(Status::InvalidArgument);
    }
    pixels.fill(0);
    let mut pixmap = tiny_skia::PixmapMut::from_bytes(pixels, width, height)
        .ok_or(Status::RenderFailed)?;
    resvg::render(
        &tree.tree,
        tiny_skia::Transform::identity(),
        &mut pixmap,
    );
    Ok(())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn mp_resvg_render(
    tree: *const MpResvgTree,
    width: u32,
    height: u32,
    pixels: *mut u8,
    pixels_len: usize,
) -> i32 {
    if tree.is_null()
        || pixels.is_null()
        || pixels_len > isize::MAX as usize
    {
        return Status::InvalidArgument as i32;
    }
    let result = catch_unwind(AssertUnwindSafe(|| {
        let tree = unsafe { &*tree };
        let pixels = unsafe { slice::from_raw_parts_mut(pixels, pixels_len) };
        render_impl(tree, width, height, pixels)
    }));
    match result {
        Ok(Ok(())) => Status::Ok as i32,
        Ok(Err(status)) => status as i32,
        Err(_) => Status::Panic as i32,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn mp_resvg_tree_destroy(tree: *mut MpResvgTree) {
    if !tree.is_null() {
        let _ = catch_unwind(AssertUnwindSafe(|| unsafe {
            drop(Box::from_raw(tree));
        }));
    }
}
