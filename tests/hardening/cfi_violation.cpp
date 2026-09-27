namespace {

class ExpectedType {
  public:
    virtual ~ExpectedType() = default;
    [[nodiscard]] virtual auto value() const -> int = 0;
};

class ActualType {
  public:
    virtual ~ActualType() = default;
    [[nodiscard]] virtual auto value() const -> int {

        return 17;
    }
};

[[gnu::noinline]] auto hide_dynamic_type(ActualType *value) -> ExpectedType * {
    void *erased = value;
    asm volatile("" : "+r"(erased) : : "memory");

    return static_cast<ExpectedType *>(erased);
}

} // namespace

auto main() -> int {
    ActualType value;

    return hide_dynamic_type(&value)->value();
}
