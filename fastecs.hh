#ifndef FASTECS_HH_
#define FASTECS_HH_

#define ECS_VERSION "0.3.3"

#include <algorithm>
#include <condition_variable>
#include <chrono>
#include <functional>
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
using      SystemPtr = int16_t;

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
    [[nodiscard]] bool has() const {
        return ecs->template has_component<C>(id, pool);
    }

    [[nodiscard]] std::string debug() const {
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
    void push_sync(const T& item, SystemPtr const& current_system)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back({ item, current_system });
    }

    void push_sync(T&& item, SystemPtr const& current_system)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back({ std::move(item), current_system });
    }

    void push_nosync(const T& item, SystemPtr const& current_system)
    {
        queue_.push_back({ item, current_system });
    }

    void push_nosync(T&& item, SystemPtr const& current_system)
    {
        queue_.push_back({ std::move(item), current_system });
    }

    std::vector<std::pair<T, SystemPtr>> const& underlying_vector() const {
        return queue_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
    }

    void clear_with_system(SystemPtr const& system_ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.erase(std::remove_if(queue_.begin(), queue_.end(),
                                [&system_ptr](std::pair<T, SystemPtr> const& t) { return t.second == system_ptr; }),
                     queue_.end());
    }

    template<typename U>
    void clear_with_type() {
        queue_.erase(std::remove_if(queue_.begin(), queue_.end(),
                     [](std::pair<T, SystemPtr> const& t) { return std::holds_alternative<U>(t.first); }),
                     queue_.end());
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::vector<std::pair<T, SystemPtr>> queue_ {};
    mutable std::mutex mutex_ {};
};

// }}}

// {{{ timer

struct SystemTime {
    std::string               name;
    std::chrono::microseconds us;
};

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

    void add_time(std::string const& name, std::chrono::microseconds us, bool mt) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& timer = mt ? _timer_mt : _timer_st;
            auto it = std::find_if(timer.begin(), timer.end(),
                                   [&name](SystemTime const& s) { return s.name == name; });
            if (it == timer.end())
                timer.push_back({ name, us });
            else
                it->us += us;
        }
        if (mt)
            add_time("multithreaded", us, false);
    }

    [[nodiscard]] std::vector<SystemTime> timer(bool mt) const {
        auto& timer = (mt ? _timer_mt : _timer_st);
        std::vector<SystemTime> t(timer.begin(), timer.end());
        for (auto& [_, us]: t)
            us /= _iterations;
        return t;
    }

private:
    std::vector<SystemTime> _timer_mt {};
    std::vector<SystemTime> _timer_st {};
    size_t _iterations = 0;
    std::mutex mutex_ {};
};

// }}}

template <typename Global, typename Message, typename Pool, typename... Components>
class ECS {
    using MyECS = ECS<Global, Message, Pool, Components...>;

public:
    using EntityType = Entity<MyECS, Pool>;
    using ConstEntityType = ConstEntity<MyECS, Pool>;

    static const char* version() { return ECS_VERSION; }

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
        _entities.emplace(_next_entity_id, DefaultPool);
        return Entity<MyECS, Pool>(_next_entity_id++, DefaultPool, this);
        // }}}
    }

    Entity<MyECS, Pool> add(Pool pool) {
        // {{{ ...
        auto it = _entity_pools.insert({ pool, {} }).first;
        it->second.emplace(_next_entity_id, pool);
        _entities.emplace(_next_entity_id, pool);
        _pool_set.insert(pool);
        _components.insert({ pool, {} });
        return Entity<MyECS, Pool>(_next_entity_id++, pool, this);
        // }}}
    }

    Entity<MyECS, Pool> get(size_t id) {
        // {{{
        auto it = _entities.find(id);
        if (it == _entities.end())
            throw ECSError("Id " + std::to_string(id) + " not found.");
        return Entity<MyECS, Pool>(id, it->second, this);
        // }}}
    }

    template <typename Component>
    Component& get(size_t id) {
        // {{{
        return get(id).template get<Component>();
        // }}}
    }

    template<typename Component>
    Component* get_ptr(size_t id) const {
        // {{{
        return get(id).template get_ptr<Component>();
        // }}}
    }

    template <typename Component>
    bool has(size_t id) {
        // {{{
        return get(id).template has<Component>();
        // }}}
    }

    ConstEntity<MyECS, Pool> get(size_t id) const {
        // {{{
        auto it = _entities.find(id);
        if (it == _entities.end())
            throw ECSError("Id " + std::to_string(id) + " not found.");
        return ConstEntity<MyECS, Pool>(id, it->second, this);
        // }}}
    }

    template <typename Component>
    Component const& get(size_t id) const {
        // {{{
        return get(id).template get<Component>();
        // }}}
    }

    bool exists(size_t id) {
        // {{{
        auto it = _entities.find(id);
        return it != _entities.end();
        // }}}
    }

    void remove(Entity<MyECS, Pool> const& entity) {
        // {{{ ...
        for (auto& [_, pool_map]: _entity_pools)
            pool_map.erase(entity.id);
        _entities.erase(entity.id);
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
        _pool_set.insert(pool);
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
            _messages.push_sync(std::move(e), _current_system);
        else
            _messages.push_nosync(std::move(e), _current_system);
        // }}}
    }

    template<typename T>
    std::vector<T> messages() const {
        // {{{ ...
        std::vector<T> r;
        for (std::pair<Message, SystemPtr> ev : _messages.underlying_vector())
            if (std::holds_alternative<T>(ev.first))
                r.push_back(std::get<T>(ev.first));
        return r;
        // }}}
    }

    template<typename T>
    std::vector<T> pop_messages() const {
        // {{{ ...
        std::vector<T> r;
        for (std::pair<Message, SystemPtr> ev : _messages.underlying_vector())
            if (std::holds_alternative<T>(ev.first))
                r.push_back(std::get<T>(ev.first));
        _messages.template clear_with_type<T>();
        return r;
    }

    void clear_messages()                       { _messages.clear(); }

    //
    // systems
    //

    void start_frame()                          { _timer.start_frame(); }
    void reset_timer()                          { _timer.reset(); }

    std::vector<SystemTime> timer_st() const { return _timer.timer(false); }
    std::vector<SystemTime> timer_mt() const { return _timer.timer(true); }

    // {{{ auxiliary methods
private:
    using Time = std::chrono::time_point<std::chrono::high_resolution_clock>;

    static Time now() { return std::chrono::high_resolution_clock::now(); }

    void add_time(std::string const& name, Time start, bool mt) const {
        _timer.add_time(name, std::chrono::duration_cast<std::chrono::microseconds>(now() - start), mt);
    }

    void update_current_system(std::string const& system_name) const {
        auto it = _system_idx.find(system_name);
        if (it == _system_idx.end()) {
            if (_system_idx.empty())
                _current_system = 0;
            else
                _current_system = std::max_element(_system_idx.begin(), _system_idx.end(),
                    [](auto const& a, auto const& b) { return a.second < b.second; })->second + 1;
            _system_idx[system_name] = _current_system;
        } else {  // more likely branch
            _current_system = it->second;
        }
    }
    // }}}

public:
    template<typename F, typename... P>
    void run_st(std::string const& name, F f, P&& ...pars) const {
        // {{{ ...
        auto start = now();
        update_current_system(name);
        _messages.clear_with_system(_current_system);
        f(*this, pars...);
        add_time(name, start, false);
        // }}}
    }

    template<typename O, typename F, typename... P, class = typename std::enable_if<std::is_class<O>::value>::type>
    void run_st(std::string const& name, O& obj, F f, P&& ...pars) const {
        // {{{ ...
        auto start = now();
        update_current_system(name);
        _messages.clear_with_system(_current_system);
        (obj.*f)(*this, pars...);
        add_time(name, start, false);
        // }}}
    }

    template<typename F, typename... P>
    void run_mutable(std::string const& name, F f, P&& ...pars) {
        // {{{ ...
        auto start = now();
        update_current_system(name);
        _messages.clear_with_system(_current_system);
        f(*this, pars...);
        add_time(name, start, false);
        // }}}
    }

    template<typename O, typename F, typename... P, class = typename std::enable_if<std::is_class<O>::value>::type>
    void run_mutable(std::string const& name, O& obj, F f, P&& ...pars) {
        // {{{ ...
        auto start = now();
        update_current_system(name);
        _messages.clear_with_system(_current_system);
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
            _threads.emplace_back([this](std::string name, MyECS const& ecs, auto f, auto... pars) {
                auto start = now();
                update_current_system(name);
                _messages.clear_with_system(_current_system);
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
            _threads.emplace_back([this](auto* obj, std::string name, MyECS const& ecs, auto f, auto&... pars) {
                auto start = now();
                update_current_system(name);
                _messages.clear_with_system(_current_system);
                std::invoke(f, obj, ecs, pars...);
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
        return std::string("{\n   global = ") + debug_global() + ",\n   entities = " + debug_entities(_pool_set, 3) + "\n}";
        // }}}
    }

    template <typename... C>
    std::string debug_entities(size_t spaces=0) const
    {
        // {{{ ...
        auto entities = find_matching_entities(_pool_set);
        std::sort(entities.begin(), entities.end());

        std::string s = "{\n";
        for (auto const& ent : entities) {
            bool print = ((ent.template has<C>() || ...));
            if (print) {
                s += std::string(spaces, ' ') + std::string("   [");
                s += std::to_string(ent.id);
                s += "] = " + ent.debug() + ",\n";
            }
        }
        return s + std::string(spaces, ' ') + "}";
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
            try {
                size += _entity_pools.at(pool).size();
            } catch (std::out_of_range&) {}
        return size;
    }

    // TODO - the methods below are repeated for const and non-const. Can this be fixed?

    template <typename Pools>
    std::vector<ConstEntity<ECS, Pool>> find_matching_entities(Pools const& pools) const {
        // {{{ ...
        size_t size = size_to_reserve(pools);
        if (size == 0)
            return {};
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
        if (size == 0)
            return {};
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
        if (size == 0)
            return {};
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
        if (size == 0)
            return {};
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
        std::string s = std::string(spaces, ' ') + "{\n";
        ((s += has_component<Components>(id, pool) ? (std::string(6, ' ') + debug_object<Components>(component<Components>(id, pool)) + "\n") : ""), ...);
        return s + std::string(3, ' ') + "}";
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

    Global                                             _global;
    Threading                                          _threading           = Threading::Multi;
    mutable SyncQueue<Message>                         _messages            {};
    std::unordered_map<size_t, Pool>                   _entities            {};
    std::unordered_map<Pool, EntityPool>               _entity_pools        { { DefaultPool, {} } };
    std::unordered_map<Pool, ComponentTupleVector>     _components          { { DefaultPool, {} } };
    size_t                                             _next_entity_id      = 0;
    std::set<Pool>                                     _pool_set            { DefaultPool };
    bool                                               _running_mt          = false;
    mutable Timer                                      _timer               {};
    mutable std::vector<std::thread>                   _threads             {};
    mutable std::unordered_map<std::string, SystemPtr> _system_idx          {};

    static inline thread_local SystemPtr               _current_system      = -1;
    static constexpr Pool DefaultPool = static_cast<Pool>(std::numeric_limits<typename std::underlying_type<Pool>::type>::max());

    // }}}
};

}

#endif

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
