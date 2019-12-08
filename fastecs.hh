#ifndef FASTECS_HH_
#define FASTECS_HH_

#include <algorithm>
#include <condition_variable>
#include <chrono> 
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <unordered_map>
#include <thread>
#include <type_traits>
#include <vector>

#ifndef NOABI
#  include <cxxabi.h>
#endif

namespace ecs {

enum class Threading { Single, Multi };
enum class NoPool {};
struct     NoGlobal {};
using      NoMessageQueue = std::variant<std::nullptr_t>;

// {{{ exception class

class ECSError : public std::runtime_error {
public:
    explicit ECSError(std::string const& what_arg) : runtime_error(what_arg) {}
    explicit ECSError(const char* what_arg)        : runtime_error(what_arg) {}
};

// }}}

// {{{ debug objects

template <typename C>
static std::string type_name() {
    // {{{ ...
    int status;
    std::string tname = typeid(C).name();
#ifndef NOABI
    char *demangled_name = abi::__cxa_demangle(tname.c_str(), nullptr, nullptr, &status);
    if(status == 0) {
        tname = demangled_name;
        free(demangled_name);
    }
#endif
    return tname;
    // }}}
}

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

template <typename Obj>
static std::string debug_object(Obj const& obj) {
    // TODO - use ['...'] when there is an invalid character
    std::stringstream ss;
    ss << type_name<Obj>() << " = {";
    if constexpr(has_ostream_method<Obj>::value)
        ss << " " << obj << " ";
    ss << "}, ";
    return ss.str();
}

// }}}

// {{{ entity classes

template<typename ECS, typename Pool>
class ConstEntity {
public:
    ConstEntity(size_t id, Pool pool, ECS const* ecs) 
        : id(id), pool(pool), ecs(ecs) {}

    template<typename C>
    C const& get() const {
        return ecs->template component<C>(id, pool);
    }

    template<typename C>
    C const* get_ptr() const {
        return ecs->template component_ptr<C>(id, pool);
    }

    template<typename C>
    bool has() const {
        return ecs->template has_component<C>(id, pool);
    }

    std::string debug() const {
        return ecs->debug_entity(id, pool);
    }

    bool operator==(ConstEntity const& other) const { return id == other.id; }
    bool operator!=(ConstEntity const& other) const { return id != other.id; }
    bool operator<(ConstEntity const& other) const { return id < other.id; }

    size_t id;   // TODO - make these two fields const
    Pool pool;

private:
    ECS const* ecs;
};


template<typename ECS, typename Pool>
class Entity : public ConstEntity<ECS, Pool> {
public:
    Entity(size_t id, Pool pool, ECS* ecs) 
        : ConstEntity<ECS, Pool>(id, pool, ecs), ecs(ecs) {}

    template<typename C, typename... P>
    C& add(P&& ...pars) {
        return ecs->template add_component<C>(this->id, this->pool, pars...);
    }

    template<typename C>
    C& get() {
        return ecs->template component<C>(this->id, this->pool);
    }

    template<typename C>
    C* get_ptr() {
        return ecs->template component_ptr<C>(this->id, this->pool);
    }

    template<typename C>
    void remove() {
        ecs->template remove_component<C>(this->id, this->pool);
    }

private:
    ECS* ecs;
};


template<typename ECS, typename Pool>
bool operator==(Entity<ECS, Pool> const& a, ConstEntity<ECS, Pool> const& b) { return a.id == b.id; }
template<typename ECS, typename Pool>
bool operator!=(Entity<ECS, Pool> const& a, ConstEntity<ECS, Pool> const& b) { return a.id != b.id; }

// }}}

// {{{ synchronized queue

template <typename T>
class SyncQueue {
public:
	void push_sync(const T& item)
	{
        std::lock_guard<std::mutex> lock(mutex_);
		queue_.push_back(item);
	}

	void push_sync(T&& item)
	{
        std::lock_guard<std::mutex> lock(mutex_);
		queue_.push_back(std::move(item));
	}

    void push_nosync(const T& item)
    {
        queue_.push_back(item);
    }

    void push_nosync(T&& item)
    {
        queue_.push_back(std::move(item));
    }

    std::vector<T> const& underlying_vector() const { 
        return queue_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::vector<T>     queue_ {};
    mutable std::mutex mutex_ {};
};

// }}}

// {{{ timer

class Timer {
public:
    void start_frame() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++_iterations;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        _timer_mt.clear();
        _timer_st.clear();
        _iterations = 0;
    }

    void add_time(std::string const& name, std::chrono::milliseconds ms, bool mt) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& timer = mt ? _timer_mt : _timer_st;
            auto it = timer.find(name);
            if (it == timer.end())
                timer.insert({ name, ms });
            else
                it->second += ms;
        }
        if (mt)
            add_time("multithreaded", ms, false);
    }

    std::map<std::string, std::chrono::milliseconds> timer(bool mt) const {
        std::map<std::string, std::chrono::milliseconds> t;
        for (auto const& [name, ms]: (mt ? _timer_mt : _timer_st))
            t.insert({ name, ms / _iterations });
        return t;
    }

private:
    std::map<std::string, std::chrono::milliseconds> _timer_mt {};
    std::map<std::string, std::chrono::milliseconds> _timer_st {};
    size_t _iterations = 0;
    std::mutex mutex_ {};
};

// }}}

template <typename Global, typename Message, typename Pool, typename... Components>
class ECS {
    using MyECS = ECS<Global, Message, Pool, Components...>;

public:
    template <typename... P>
    explicit ECS(P&& ...pars) 
        : _global(Global { pars... }) {}

    ~ECS() { join(); }

    void set_threading(Threading t)         { _threading = t; }

    // 
    // entities
    //

    Entity<MyECS, Pool> add() {
        // {{{ ...
        _entity_pools.at(DefaultPool).emplace(_next_entity_id, DefaultPool);
        return Entity<MyECS, Pool>(_next_entity_id++, DefaultPool, this);
        // }}}
    }

    Entity<MyECS, Pool> add(Pool pool) { 
        // {{{ ...
        auto it = _entity_pools.insert({ pool, {} }).first;
        it->second.emplace(_next_entity_id, pool);
        _pool_set.insert(pool);
        _components.insert({ pool, {} });
        return Entity<MyECS, Pool>(_next_entity_id++, pool, this);
        // }}}
    }

    void remove(Entity<MyECS, Pool> const& entity) {
        // {{{ ...
        for (auto& [_, pool_map]: _entity_pools)
            pool_map.erase(entity.id);
        // }}}
    }

    //
    // iteration
    //

    std::vector<Entity<ECS, Pool>> entities() {
        // {{{ ...
        return find_matching_entities(_pool_set);
        // }}}
    }

    std::vector<Entity<ECS, Pool>> entities(Pool pool) {
        // {{{ ...
        return find_matching_entities(std::vector<Pool> { pool });
        // }}}
    }

    template <typename... T>
    std::vector<Entity<ECS, Pool>> entities() {
        // {{{ ...
        return find_matching_entities_component<T...>(_pool_set);
        // }}}
    }

    template <typename... T>
    std::vector<Entity<ECS, Pool>> entities(Pool pool) {
        // {{{ ...
        return find_matching_entities_component<T...>(std::vector<Pool> { pool });
        // }}}
    }

    std::vector<ConstEntity<ECS, Pool>> entities() const {
        // {{{ ...
        return find_matching_entities(_pool_set);
        // }}}
    }

    std::vector<ConstEntity<ECS, Pool>> entities(Pool pool) const {
        // {{{ ...
        return find_matching_entities(std::vector<Pool> { pool });
        // }}}
    }

    template <typename... T>
    std::vector<ConstEntity<ECS, Pool>> entities() const {
        // {{{ ...
        return find_matching_entities_component<T...>(_pool_set);
        // }}}
    }

    template <typename... T>
    std::vector<ConstEntity<ECS, Pool>> entities(Pool pool) const {
        // {{{ ...
        return find_matching_entities_component<T...>(std::vector<Pool> { pool });
        // }}}
    }

    //
    // globals
    //

    Global& operator()()                        { return _global; }
    Global const& operator()() const            { return _global; }

    //
    // messages 
    //

    void add_message(Message&& e) const { 
        // {{{ ...
        if (_running_mt)
            _messages.push_sync(std::move(e));
        else
            _messages.push_nosync(std::move(e));
        // }}}
    }
        
    template<typename T>
    std::vector<T> messages() {   // non-const by design
        // {{{ ...
        std::vector<T> r;
        for (auto ev : _messages.underlying_vector())
            if (std::holds_alternative<T>(ev))
                r.push_back(std::get<T>(ev));
        return r;
        // }}}
    }

    void clear_messages()                       { _messages.clear(); }

    //
    // systems
    //
    
    void start_frame()                          { _timer.start_frame(); }
    void reset_timer()                          { _timer.reset(); }

    std::map<std::string, std::chrono::milliseconds> timer_st() const { return _timer.timer(false); }
    std::map<std::string, std::chrono::milliseconds> timer_mt() const { return _timer.timer(true); }

    // {{{ auxiliary methods
private:
    using Time = std::chrono::time_point<std::chrono::high_resolution_clock>;
    static Time now() { return std::chrono::high_resolution_clock::now(); }
    void add_time(std::string const& name, Time start, bool mt) const { 
        _timer.add_time(name, std::chrono::duration_cast<std::chrono::milliseconds>(now() - start), mt);
    }
    // }}}
    
public:
    template<typename F, typename... P>
    void run_st(std::string const& name, F f, P&& ...pars) const {
        // {{{ ...
        auto start = now();
        f(*this, pars...);
        add_time(name, start, false);
        // }}}
    }

    template<typename O, typename F, typename... P, class = typename std::enable_if<std::is_class<O>::value>::type>
    void run_st(std::string const& name, O& obj, F f, P&& ...pars) const { 
        // {{{ ...
        auto start = now();
        (obj.*f)(*this, pars...);  
        add_time(name, start, false);
        // }}}
    }

    template<typename F, typename... P>
    void run_mutable(std::string const& name, F f, P&& ...pars) {
        // {{{ ...
        auto start = now();
        f(*this, pars...);
        add_time(name, start, false);
        // }}}
    }

    template<typename O, typename F, typename... P, class = typename std::enable_if<std::is_class<O>::value>::type>
    void run_mutable(std::string const& name, O& obj, F f, P&& ...pars) { 
        // {{{ ...
        auto start = now();
        (obj.*f)(*this, pars...);  
        add_time(name, start, false);
        // }}}
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"

    template<typename F, typename... P>
    void run_mt(std::string const& name, F f, P&& ...pars) const {
        // {{{ ...
        static_assert(!(((std::is_reference_v<P> && !std::is_const_v<P>) || ...)), 
                "Don't use non-const references in multithreaded code. Use pointers instead.");
        if (_threading == Threading::Single) {
            run_st(name, f, pars...);
        } else {
            _threads.emplace_back([](std::string name, MyECS const& ecs, auto f, auto... pars) {
                auto start = now();
                f(ecs, pars...);
                ecs.add_time(name, start, true);
            }, name, std::ref(*this), f, pars...);
        }
        // }}}
    }

    template<typename O, typename F, typename... P, class = typename std::enable_if<std::is_class<O>::value>::type>
    void run_mt(std::string const& name, O& obj, F f, P&& ...pars) const { 
        // {{{ ...
        static_assert(!(((std::is_reference_v<P> && !std::is_const_v<P>) || ...)), 
                "Don't use non-const references in multithreaded code. Use pointers instead.");
        if (_threading == Threading::Single) {
            run_st(name, obj, f, pars...);
        } else {
            _threads.emplace_back([](auto* obj, std::string name, MyECS const& ecs, auto f, auto&... pars) {
                auto start = now();
                (obj.*f)(ecs, pars...);
                ecs.add_time(name, start, true);
            }, &obj, name, std::ref(*this), f, pars...);
        }
        // }}}
    }

#pragma GCC diagnostic push

    void join() {
        // {{{ ...
        for (std::thread& t: _threads)
            t.join();
        _threads.clear();
        // }}}
    }

    friend class ConstEntity<ECS, Pool>;
    friend class Entity<ECS, Pool>;

    // 
    // debugging
    //

    std::string debug_entities(size_t spaces=0) const            { return debug_entities(_pool_set, spaces); }
    std::string debug_entities(Pool pool, size_t spaces=0) const { return debug_entities(std::vector<Pool> { pool }, spaces); }

    std::string debug_global() const {
        // {{{ ...
        std::stringstream ss;
        ss << "{ " << _global << " }";
        return ss.str();
        // }}}
    }

    std::string debug_all() const {
        // {{{
        return std::string("{\n   global = ") + debug_global() + ",\n"
            "   entities = " + debug_entities(_pool_set, 3) + "\n}";
        // }}}
    }

    size_t number_of_entities() const               { return size_to_reserve(_pool_set); }
    size_t number_of_components() const             { return std::tuple_size<ComponentTupleVector>::value; }
    size_t number_of_message_types() const {
        // {{{ ...
        if constexpr (std::is_same<Message, NoMessageQueue>::value)
            return 0;
        return std::variant_size_v<Message>;
        // }}}
    }
    size_t message_queue_size() const               { return _messages.size(); }

private:

    // {{{ templates & static assertions
    
#if __cplusplus < 201703L
#error "A compiler with C++17 support is required."
#endif

    // create a tuple from the component list

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"

    using ComponentTupleVector = typename std::tuple<std::vector<std::pair<size_t, Components>>...>;
    static_assert(std::tuple_size<ComponentTupleVector>::value > 0, "Add at least one component.");

    static_assert((std::is_copy_constructible<Components>::value, ...), "All components must be copyable.");

    template <typename>      struct is_std_variant : std::false_type {};
    template <typename... T> struct is_std_variant<std::variant<T...>> : std::true_type {};
    static_assert(is_std_variant<Message>::value, "Message must be a std::variant<...>.");

#pragma GCC diagnostic pop

    // check if component is on the list
    template <typename C>
    void check_component() const {
        static_assert((std::is_same_v<C, Components> || ...), "This component is not part of the component list given in the Engine initialization.");
    }

    // check if pool is an enum
    static_assert(std::is_enum_v<Pool>, "Pool must be an enum.");

    // }}}

    // {{{ private methods (iteration)

    // type aliases
    template <typename C>
    using my_iter = typename std::vector<std::pair<size_t, C>>::iterator;

    template <typename C>
    using my_citer = typename std::vector<std::pair<size_t, C>>::const_iterator;

    template <typename Pools>
    size_t size_to_reserve(Pools const& pools) const {
        size_t size = 0;
        for (auto const& pool: pools)
            size += _entity_pools.at(pool).size();
        return size;
    }

    // TODO - the methods below are repeated for const and non-const. Can this be fixed?

    template <typename Pools>
    std::vector<ConstEntity<ECS, Pool>> find_matching_entities(Pools const& pools) const {
        // {{{ ...
        size_t size = size_to_reserve(pools);
        std::vector<ConstEntity<ECS, Pool>> entities;
        entities.reserve(size);
        for (auto const& pool: pools)
            for (auto [id,_]: _entity_pools.at(pool))
                entities.emplace_back(id, pool, this);
        return entities;
        // }}}
    }
    
    template <typename Pools>
    std::vector<Entity<ECS, Pool>> find_matching_entities(Pools const& pools) {
        // {{{ ...
        size_t size = size_to_reserve(pools);
        std::vector<Entity<ECS, Pool>> entities;
        entities.reserve(size);
        for (auto const& pool: pools)
            for (auto [id,_]: _entity_pools.at(pool))
                entities.emplace_back(id, pool, this);
        return entities;
        // }}}
    }
    
    // TODO - the methods below are repeated for const and non-const. Can this be fixed?

    template <typename... C, typename Pools>
    std::vector<Entity<ECS, Pool>> find_matching_entities_component(Pools const& pools) {
        // {{{ ...
        ((check_component<C>(), ...));

        size_t size = size_to_reserve(pools);
        std::vector<Entity<ECS, Pool>> entities;
        entities.reserve(size);
        for (auto const& pool: pools) {
            if constexpr (sizeof...(C) == 0) {
                for (auto [id,_]: _entity_pools.at(pool))
                    entities.emplace_back(id, pool, this);
            } else {
                // initialize a tuple of iterators, each one pointing to the initial iterator of its component vector
                std::tuple<my_iter<C>...> current;
                ((std::get<my_iter<C>>(current) = comp_vec<C>(pool).begin()), ...);
                
                // while none of the iterators reached end
                while (((std::get<my_iter<C>>(current) != comp_vec<C>(pool).end()) && ...)) {
                    // find iterator that is more advanced
                    std::vector<size_t> entities1 { std::get<my_iter<C>>(current)->first... };
                    [[maybe_unused]] size_t last = *std::max_element(entities1.begin(), entities1.end());

                    // advance all iterators that are behind the latest one
                    (((std::get<my_iter<C>>(current)->first < last) ? std::get<my_iter<C>>(current)++ : std::get<my_iter<C>>(current)), ...);
                    if (((std::get<my_iter<C>>(current) == comp_vec<C>(pool).end()) || ...))
                        break;

                    // if all iterators are equal, call user function and advance all iterators
                    std::vector<size_t> entities2 { std::get<my_iter<C>>(current)->first... };
                    if (std::adjacent_find(entities2.begin(), entities2.end(), std::not_equal_to<size_t>()) == entities2.end()) {
                        entities.emplace_back(entities2.at(0), pool, this);
                        (std::get<my_iter<C>>(current)++, ...);
                    }
                }
            }
        }
        return entities;
        // }}}
    }

    template <typename... C, typename Pools>
    std::vector<ConstEntity<ECS, Pool>> find_matching_entities_component(Pools const& pools) const {
        // {{{ ...
        ((check_component<C>(), ...));

        size_t size = size_to_reserve(pools);
        std::vector<ConstEntity<ECS, Pool>> entities;
        entities.reserve(size);
        for (auto const& pool: pools) {
            if constexpr (sizeof...(C) == 0) {
                for (auto [id,_]: _entity_pools.at(pool))
                    entities.emplace_back(id, pool, this);
            } else {
                // initialize a tuple of iterators, each one pointing to the initial iterator of its component vector
                std::tuple<my_citer<C>...> current;
                ((std::get<my_citer<C>>(current) = comp_vec<C>(pool).cbegin()), ...);
                
                // while none of the iterators reached cend
                while (((std::get<my_citer<C>>(current) != comp_vec<C>(pool).cend()) && ...)) {
                    // find iterator that is more advanced
                    std::vector<size_t> entities1 { std::get<my_citer<C>>(current)->first... };
                    [[maybe_unused]] size_t last = *std::max_element(entities1.cbegin(), entities1.cend());

                    // advance all iterators that are behind the latest one
                    (((std::get<my_citer<C>>(current)->first < last) ? std::get<my_citer<C>>(current)++ : std::get<my_citer<C>>(current)), ...);
                    if (((std::get<my_citer<C>>(current) == comp_vec<C>(pool).cend()) || ...))
                        break;

                    // if all iterators are equal, call user function and advance all iterators
                    std::vector<size_t> entities2 { std::get<my_citer<C>>(current)->first... };
                    if (std::adjacent_find(entities2.cbegin(), entities2.cend(), std::not_equal_to<size_t>()) == entities2.cend()) {
                        entities.emplace_back(entities2.at(0), pool, this);
                        (std::get<my_citer<C>>(current)++, ...);
                    }
                }
            }
        }
        return entities;
        // }}}
    }

    // }}}

    // {{{ private methods (components)

    template<typename C, typename... P>
    C& add_component(size_t id, Pool pool, P&& ...pars) {
        // {{{ ...
        check_component<C>();

        auto& vec = comp_vec<C>(pool);
        auto it = std::lower_bound(begin(vec), end(vec), id,
            [](auto const& p, auto e) { return p.first < e; });

        if (it != vec.end() && it->first == id)
            throw ECSError(std::string("Component '") + type_name<C>() + "' already exist for entity " + std::to_string(id) + ".");

        return vec.emplace(it, id, C { pars... })->second;
        // }}}
    }

    template<typename C>
    C& component(size_t id, Pool pool) {
        // {{{ ...
        return const_cast<C&>(static_cast<MyECS const*>(this)->component<C>(id, pool));
    }

    template<typename C>
    C const& component(size_t id, Pool pool) const {
        C const* c = component_ptr<C>(id, pool);
        if (c == nullptr)
            throw ECSError(std::string("Entity ") + std::to_string(id) + " has no component '" + type_name<C>() + "'.");
        return *c;
        // }}}
    }

    template<typename C>
    C* component_ptr(size_t id, Pool pool) {
        // {{{ ...
        return const_cast<C*>(static_cast<MyECS const*>(this)->component_ptr<C>(id, pool));
    }

    template<typename C>
    C const* component_ptr(size_t id, Pool pool) const {
        check_component<C>();

        auto& vec = comp_vec<C>(pool);
        auto it = std::lower_bound(begin(vec), end(vec), id,
            [](auto const& p, size_t e) { return p.first < e; });
        if (it != vec.end() && it->first == id)
            return &it->second;
        return nullptr;
        // }}}
    }

    template<typename C>
    bool has_component(size_t id, Pool pool) const {
        // {{{ ...
        return component_ptr<C>(id, pool) != nullptr;
        // }}}
    }

    template<typename C>
    void remove_component(size_t id, Pool pool) {
        // {{{ ...
        check_component<C>();

        auto& vec = comp_vec<C>(pool);
        auto it = std::lower_bound(begin(vec), end(vec), id,
            [](auto const& p, size_t e) { return p.first < e; });
        if (it != vec.end() && it->first == id)
            vec.erase(it);
        else
            throw ECSError(std::string("Entity ") + std::to_string(id) + " has no component '" + type_name<C>() + "'.");
        // }}}
    }

    template <typename C>
    std::vector<std::pair<size_t, C>>& comp_vec(Pool pool) {
        // {{{ ...
        return std::get<std::vector<std::pair<size_t, C>>>(_components.at(pool));
    }

    template <typename C>
    std::vector<std::pair<size_t, C>> const& comp_vec(Pool pool) const {
        return std::get<std::vector<std::pair<size_t, C>>>(_components.at(pool));
        // }}}
    }

    // }}}

    // {{{ private methods (debugging)

    template <typename C>
    std::string debug_component(size_t id) const 
    {
        check_component<C>();
        return "{ " + debug_object<C>(component<C>(id)) + "}";
    }

    std::string debug_entity(size_t id, Pool pool, size_t spaces=0) const
    {
        std::string s = std::string(spaces, ' ') + "{ ";
        ((s += has_component<Components>(id, pool) ? debug_object<Components>(component<Components>(id, pool)) : ""), ...);
        return s + "}";
    }

    template <typename T>
    std::string debug_entities(T const& pools, size_t spaces=0) const
    {
        // {{{ ...
        auto entities = find_matching_entities(pools);
        std::sort(entities.begin(), entities.end());

        std::string s = "{\n";
        for (auto const& ent : entities) {
            s += std::string(spaces, ' ') + std::string("   [");
            s += std::to_string(ent.id);
            s += "] = " + ent.debug() + ",\n";
        }
        return s + std::string(spaces, ' ') + "}";
        // }}}
    }

    // }}}

    // {{{ private data

    using EntityPool = std::unordered_map<size_t, Pool>;

    Global                                         _global;
    Threading                                      _threading           = Threading::Multi;
    mutable SyncQueue<Message>                     _messages            {};
    std::unordered_map<Pool, EntityPool>           _entity_pools        { { DefaultPool, {} } };
    std::unordered_map<Pool, ComponentTupleVector> _components          { { DefaultPool, {} } };
    size_t                                         _next_entity_id      = 0;
    std::set<Pool>                                 _pool_set            { DefaultPool };
    bool                                           _running_mt          = false;
    mutable Timer                                  _timer               {};
    mutable std::vector<std::thread>               _threads             {};

    static constexpr Pool DefaultPool = static_cast<Pool>(std::numeric_limits<typename std::underlying_type<Pool>::type>::max());

    // }}}
};

}

#endif

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
