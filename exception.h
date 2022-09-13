#ifndef REMOTE_DESKTOP_EXCEPTION_H
#define REMOTE_DESKTOP_EXCEPTION_H

class InitFail : public std::exception {
public:
    const char *msg;

    explicit InitFail(const char *msg) : msg(msg) {};
    ~InitFail() override = default;

    [[nodiscard]] const char* what() const noexcept override {
        return msg;
    }
};

class RunError : public std::exception {
public:
    const char *msg;

    explicit RunError(const char *msg) : msg(msg) {};
    ~RunError() override = default;

    [[nodiscard]] const char* what() const noexcept override {
        return msg;
    }
};

#endif //REMOTE_DESKTOP_EXCEPTION_H
