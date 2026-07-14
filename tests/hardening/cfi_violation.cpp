namespace {

class ExpectedType {
public:
    virtual ~ExpectedType() = default;
    virtual int value() const = 0;
};

class ActualType {
public:
    virtual ~ActualType() = default;
    virtual int value() const
    {
        return 17;
    }
};

[[gnu::noinline]] ExpectedType* hide_dynamic_type(ActualType* value)
{
    void* erased = value;
    asm volatile("" : "+r"(erased) : : "memory");
    return static_cast<ExpectedType*>(erased);
}

} // namespace

int main()
{
    ActualType value;
    return hide_dynamic_type(&value)->value();
}
