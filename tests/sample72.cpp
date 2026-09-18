// Dedicated test for VIRCON32_QUIRKS.md entry #12's GENERAL case: a
// function returning a pointer or reference type, independent of operator
// overloading entirely (sample9.cpp/sample15.cpp already exercise the
// operator-specific angle -- this file exists so the underlying grammar/
// lowering fix is also verified on its own, ordinary-function terms).
//
// Covers all four combinations this fix needs to get right:
//   - an in-class method DECLARING a pointer return (getSelfPointer)
//   - that same method's OUT-OF-LINE definition also using pointer_opt
//     (func_header's grammar and out_of_line_def's are two separate
//     productions -- both need the fix, not just one)
//   - a free function returning a pointer (makeBox)
//   - a method returning a REFERENCE (getValueRef), which lowers to a
//     pointer return PLUS an implicit address-of at every `return expr`
//     site (inject_reference_return_address_stmt) PLUS the function's
//     own return-type relabeling (fix_references_in_method) -- the two
//     halves of the reference-return fix, both exercised here.

class Box {
    public:
        Box(int value);
        Box *getSelfPointer();
        int &getValueRef();
        int value;
};

Box::Box(int value) {
    this->value = value;
}

Box *Box::getSelfPointer() {
    return this;
}

int &Box::getValueRef() {
    return value;
}

Box *makeBox(int value) {
    Box *b = new Box(value);
    return b;
}

void main() {
    // NOTE: stack direct-initialization with constructor arguments
    // (`Box original(7);`) is a separate, still-unimplemented gap
    // (unrelated to this file's own pointer/reference-return fix) --
    // see the TODO in docs/VIRCON32_QUIRKS.md. Using `new` here instead
    // sidesteps it cleanly, since heap allocation already goes through
    // the constructor-argument-forwarding path new/delete lowering
    // supports.
    Box *original = new Box(7);

    int viaPointerMethod = original->getSelfPointer()->value;

    Box *madeElsewhere = makeBox(7);
    int viaFreeFunction = madeElsewhere->value;

    int viaReference = original->getValueRef();
}
