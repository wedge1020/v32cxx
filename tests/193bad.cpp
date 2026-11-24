// two enums of one name in two namespaces: their C type names are a__E and b__E (once this was a sema error)
namespace a { enum E { X }; }
namespace b { enum E { Y }; }
int main(void) { return 0; }
