#ifndef FASTECS_HH_
#define FASTECS_HH_

#include <chrono> 
#include <map>
#include <string>
#include <variant>
#include <thread>
#include <type_traits>

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
    using MyECS = ECS<Global, Event, Pool, Components...>;

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

    void           add_message(Event&& e) const {}  // TODO
    template<typename T>
    std::vector<T> messages() const {}  // TODO - change container type
    void           clear_messages() {}  // TODO

    // 
    // systems
    //
    
    void start_frame() {}  // TODO
    void reset_timer() {}  // TODO

    std::map<std::string, size_t> timer_st() const {}
    std::map<std::string, size_t> timer_mt() const {}

    template<typename F, typename... P>
    void run_st(std::string const& name, F f, P&& ...pars) const {
        f(*this, pars...);
    }  // TODO

    template<typename O, typename F, typename... P,
        class = typename std::enable_if<std::is_class<O>::value>::type>
    void run_st(std::string const& name, O& obj, F f, P&& ...pars) const { 
        (obj.*f)(*this, pars...);  
    }  // TODO

    template<typename F, typename... P>
    void run_mutable(std::string const& name, F f, P&& ...pars) {
        f(*this, pars...);
    }  // TODO

    template<typename O, typename F, typename... P,
        class = typename std::enable_if<std::is_class<O>::value>::type>
    void run_mutable(std::string const& name, O& obj, F f, P&& ...pars) { 
        (obj.*f)(*this, pars...);  
    }  // TODO

    template<typename F, typename... P>
    void run_mt(std::string const& name, F f, P&& ...pars) const {
        f(*this, pars...);
    }  // TODO

    template<typename O, typename F, typename... P,
        class = typename std::enable_if<std::is_class<O>::value>::type>
    void run_mt(std::string const& name, O& obj, F f, P&& ...pars) const { 
        (obj.*f)(*this, pars...);  
    }  // TODO

    void join() {}  // TODO

    // 
    // private data
    //
private:
    // {{{ templates & static assertions
    
    // create a tuple from the component list

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"

    using ComponentTupleVector = typename std::tuple<std::vector<std::pair<Entity, Components>>...>;
    static_assert(std::tuple_size<ComponentTupleVector>::value > 0, "Add at least one component.");

    static_assert((std::is_copy_constructible<Components>::value, ...), "All components must be copyable.");

    template <typename>      struct is_std_variant : std::false_type {};
    template <typename... T> struct is_std_variant<std::variant<T...>> : std::true_type {};
    static_assert(is_std_variant<Event>::value, "Event must be a std::variant<...>.");

#pragma GCC diagnostic pop

    // check if component is on the list
    template <typename C>
    void check_component() const {
        static_assert((std::is_same_v<C, Components> || ...), "This component is not part of the component list given in the Engine initialization.");
    }

    // check if pool is an enum
    static_assert(std::is_enum_v<Pool>, "Pool must be an enum.");

    // check if class has operator<<
    template<typename T>
    struct has_ostream_method
    {
    private:
        typedef std::true_type  yes;
        typedef std::false_type no;

        template<typename U> static auto test(int) -> decltype(std::declval<std::ostream&>() << std::declval<U>(), yes());
        template<typename> static no test(...);
    public:
        static constexpr bool value = std::is_same<decltype(test<T>(0)),yes>::value;
    };

    // }}}

    Global               _global              {};
    std::vector<Event>   _events              {};
    ComponentTupleVector _components_active   {};
    size_t               _next_entity_id      = 0;

};

}

#endif

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
