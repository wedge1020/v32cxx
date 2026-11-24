// goto may not jump past the declaration of an object with a constructor
// (C++ [stmt.dcl]): forward in one block, and into a block.
class D {
public:
    int id;
    D(int i) { id = i; }
    ~D() { }
};

void forward_same_block(void)
{
    goto over;
    D d(1);
over:
    ;
}

void into_block_past_decl(void)
{
    goto inside;
    {
        D d(2);
inside:
        ;
    }
}

int main(void)
{
    forward_same_block();
    into_block_past_decl();
    return 0;
}
