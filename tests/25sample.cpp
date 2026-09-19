// Exercises destructor invocation via `delete` (lower.c's AST_DELETE
// naming + codegen.c's emit_delete_runtime) -- none of this project's
// existing tests declare a destructor WITH a body (tests/sample7.cpp's
// ~Shape() is prototype-only, same gap tests/sample24.cpp closed for
// vtable instances last round), so none of them would exercise the
// "actually call it" path at all. Deliberately no virtual methods here
// -- this test is scoped to just constructor+destructor invocation,
// kept separate from the vtable concerns sample24.cpp already covers.

class Logger {
    public:
        Logger(int id);
        ~Logger();
    private:
        int id;
};

Logger::Logger(int id) {
    this->id = id;
}

Logger::~Logger() {
    this->id = 0;
}

void main() {
    Logger *logger = new Logger(7);
    delete logger;
}
