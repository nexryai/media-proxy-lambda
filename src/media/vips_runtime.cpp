#include <mediaproxy/media/vips_runtime.hpp>

#include <vips/vips.h>

namespace mediaproxy::media {
namespace {

class VipsRuntime final {
public:
    VipsRuntime() noexcept
        : initialized_(vips_init("mediaproxy-lambda") == 0)
    {
        if (!initialized_) {
            return;
        }
        vips_concurrency_set(vips_worker_concurrency);
        vips_cache_set_max_mem(vips_cache_maximum_memory);
        vips_cache_set_max(vips_cache_maximum_entries);
        vips_cache_set_max_files(vips_cache_maximum_files);
    }

    ~VipsRuntime()
    {
        if (initialized_) {
            vips_shutdown();
        }
    }

    VipsRuntime(const VipsRuntime&) = delete;
    VipsRuntime& operator=(const VipsRuntime&) = delete;

    [[nodiscard]] bool initialized() const noexcept
    {
        return initialized_;
    }

private:
    bool initialized_;
};

} // namespace

bool initialize_vips() noexcept
{
    static const VipsRuntime runtime;
    return runtime.initialized();
}

} // namespace mediaproxy::media
