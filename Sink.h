#ifndef REMOTE_DESKTOP_SINK_H
#define REMOTE_DESKTOP_SINK_H

#include <memory>

template<class T>
class Sink {
protected:
    Sink() = default;
    virtual ~Sink() = default;

public:
    virtual void handle(T *t) = 0;
};

#endif //REMOTE_DESKTOP_SOURCE_H
