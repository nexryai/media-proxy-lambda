#include <mediaproxy/media/vips_runtime.hpp>

#include <vips/vips.h>

namespace mediaproxy::media {
namespace {

class VipsRuntime final {
  public:
    VipsRuntime() noexcept : initialized_(vips_init("mediaproxy-lambda") == 0) {
        if (!initialized_) {
            return;
        }
        vips_concurrency_set(vips_worker_concurrency);
        vips_cache_set_max_mem(vips_cache_maximum_memory);
        vips_cache_set_max(vips_cache_maximum_entries);
        vips_cache_set_max_files(vips_cache_maximum_files);
    }

    ~VipsRuntime() {
        if (initialized_) {
            vips_shutdown();
        }
    }

    VipsRuntime(const VipsRuntime &) = delete;
    auto operator=(const VipsRuntime &) -> VipsRuntime & = delete;

    [[nodiscard]] auto initialized() const noexcept -> bool {

        return initialized_;
    }

  private:
    bool initialized_;
};

} // namespace

auto initialize_vips() noexcept -> bool {
    static const VipsRuntime runtime;

    return runtime.initialized();
}

} // namespace mediaproxy::media
