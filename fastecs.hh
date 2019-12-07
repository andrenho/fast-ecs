#ifndef FASTECS_HH_
#define FASTECS_HH_

#include <functional>
#include <variant>

#if __cplusplus < 201703L
#error "A compiler with C++17 support is required."
#endif

namespace ecs {

enum class Threading { Single, Multi };
enum class NoPool {};

struct NoGlobal {};
using  NoEventQueue = std::variant<std::nullptr_t>;

class Entity {
public:
    Entity(size_t id) : id(id) {}
    const size_t id;

    template<typename T, typename... P>
    void add(P&& ...pars) { }  // TODO

    template<typename T>
    T& get() {}

    template<typename T>
    T const& get() const {}

    template<typename T>
    T* get_ptr() {}

    template<typename T>
    T const* get_ptr() const {}

    template<typename T>
    bool has() const {}

    template<typename T>
    void remove() {}

    bool operator==(Entity const& other) const { return id == other.id; }
    bool operator!=(Entity const& other) const { return id != other.id; }
};

class EntityIterator {
public:
    EntityIterator operator++(int);   // TODO
    Entity& operator*() const;   // TODO
    friend bool operator==(const EntityIterator&, const EntityIterator&);   // TODO
    friend bool operator!=(const EntityIterator&, const EntityIterator&);    // TODO
    EntityIterator& operator++() {}   // TODO
    EntityIterator begin() {}   // TODO
    EntityIterator end() {}   // TODO
};

class ConstEntityIterator {
public:
    ConstEntityIterator operator++(int);   // TODO
    Entity const& operator*() const;   // TODO
    friend bool operator==(const ConstEntityIterator&, const ConstEntityIterator&);   // TODO
    friend bool operator!=(const ConstEntityIterator&, const ConstEntityIterator&);    // TODO
    ConstEntityIterator& operator++() {}   // TODO
    ConstEntityIterator begin() {}   // TODO
    ConstEntityIterator end() {}   // TODO
};

template <typename Global, typename Event, typename Pool, typename... Components>
class ECS {
public:
    template <typename... P>
    explicit ECS(Threading threading, P&& ...pars) : _global(Global { pars... }) {}

    //
    // entities
    //

    size_t number_of_entities() const { return 0; }   // TODO
    Entity add() {}   // TODO
    Entity add(Pool pool) { (void) pool; }  // TODO
    void   remove(Entity const& entity) {}   // TODO

    //
    // iteration
    // 

    EntityIterator entities() {}   // TODO
    EntityIterator entities(Pool pool) {}  // TODO

    template <typename... T>
    EntityIterator entities() {}   // TODO
    template <typename... T>
    EntityIterator entities(Pool pool) {}  // TODO

    ConstEntityIterator entities() const {}   // TODO
    ConstEntityIterator entities(Pool pool) const {}  // TODO

    template <typename... T>
    ConstEntityIterator entities() const {}   // TODO
    template <typename... T>
    ConstEntityIterator entities(Pool pool) const {}  // TODO

    // 
    // globals
    //

    Global& operator()()             { return _global; }
    Global const& operator()() const { return _global; }

    //
    // messages
    //

    void           add_message(Event&& e) {}  // TODO
    template<typename T>
    std::vector<T> messages() const {}  // TODO - change container type
    void           clear_messages() {}  // TODO

    // 
    // systems
    //
    
    void start_frame() {}  // TODO

    template<typename F, typename... P>
    void run_st(F f, P&& ...pars) {}  // TODO

    template<typename O, typename F, typename... P>
    void run_st(O const& obj, F f, P&& ...pars) {}  // TODO

    // 
    // private data
    //
private:
    Global _global;
};

}

#endif

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
