#ifndef FASTECS_HH_
#define FASTECS_HH_

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <tuple>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#ifndef NOABI
#  include <cxxabi.h>
#endif

namespace ecs {

class ECSError : public std::runtime_error {
public:
    explicit ECSError(std::string const& what_arg) : runtime_error(what_arg) {}
    explicit ECSError(const char* what_arg)        : runtime_error(what_arg) {}
};

struct NoSystem {};
struct NoGlobal {};
using  NoEventQueue = std::variant<>;

struct Entity {
    size_t get()                          { return value; }
    // {{{ ...
    explicit Entity(size_t value)           : value(value) {}
    explicit Entity(Entity&& entity)        : value(std::move(entity.value)) {}
    size_t   get() const                    { return value; }

    Entity(Entity const& e)                 : value(e.get()) {}
    Entity& operator=(Entity const& e)      { value = e.value; return *this; }

    bool operator==(Entity const& e) const  { return value == e.get(); }
    bool operator<(Entity const& e) const   { return value < e.get(); }

private:
    size_t   value;
    // }}}
};

using EntityOrName = std::variant<Entity, std::string>;

template <typename System, typename Global, typename Event, typename... Components>
class Engine {
public:

    // entities

    Entity   add_entity(std::optional<std::string> const& name = {});

    bool     is_entity_active(EntityOrName const& ent) const;
    void     set_entity_active(EntityOrName const& ent, bool active);

    std::optional<std::string> entity_debugging_info(EntityOrName const& ent) const;
    void                       set_entity_debugging_info(EntityOrName const& ent, std::string const& text);

    void     remove_entity(EntityOrName const& ent);

    // components

    template <typename C>
    C&       add_component(EntityOrName const& ent, C&& c) {
        // {{{ ...
        Entity id = entity(ent);
        auto& vec = comp_vec<C>(_entities.at(id));
        auto it = std::lower_bound(begin(vec), end(vec), id,
            [](auto const& p, Entity const& e) { return p.first < e; });

        if (it != vec.end() && it->first == id)
            throw ECSError(std::string("Component '") + component_name<C>() + "' already exist for entity " + std::to_string(id.get()) + ".");
        return vec.insert(it, { id, c })->second;
        // }}}
    }

    template <typename C, typename... P>
    C&       add_component(EntityOrName const& ent, P&& ...pars) {
        // {{{ ...
        return add_component(ent, C { pars... });
        // }}}
    }

    template <typename C>
    bool     has_component(EntityOrName const& ent) const {
        // {{{ ...
        return component_ptr<C>(ent) != nullptr;
        // }}}
    }

    template <typename C>
    C&       component(EntityOrName const& ent) {
        // {{{ ...
        return const_cast<C&>(static_cast<const Engine<System, Global, Event, Components...>*>(this)->component<C>(ent));
        // }}}
    }

    template <typename C>
    C const& component(EntityOrName const& ent) const {
        // {{{ ...
        C const* c = component_ptr<C>(ent);
        if (c == nullptr)
            throw ECSError(std::string("Entity ") + std::to_string(entity(ent).get()) + " has no component '" + component_name<C>() + "'.");
        return *c;
        // }}}
    }

    template <typename C>
    C*       component_ptr(EntityOrName const& ent) {
        // {{{ ...
        return const_cast<C*>(static_cast<const Engine<System, Global, Event, Components...>*>(this)->component_ptr<C>(ent));
        // }}}
    }

    template <typename C>
    C const* component_ptr(EntityOrName const& ent) const {
        // {{{ ...
        Entity id = entity(ent);
        auto& vec = comp_vec<C>(_entities.at(id));
        auto it = std::lower_bound(begin(vec), end(vec), id,
            [](auto const& p, Entity const& e) { return p.first < e; });
        if (it != vec.end() && it->first == id)
            return &it->second;
        return nullptr;
        // }}}
    }

    template <typename C>
    void     remove_component(EntityOrName const& ent) {
        // {{{ ...
        Entity id = entity(ent);
        auto& vec = comp_vec<C>(_entities.at(id));
        auto it = std::lower_bound(begin(vec), end(vec), id,
            [](auto const& p, Entity const& e) { return p.first < e; });
        if (it != vec.end() && it->first == id)
            vec.erase(it);
        else
            throw ECSError(std::string("Entity ") + std::to_string(id.get()) + " has no component '" + component_name<C>() + "'.");
        // }}}
    }

    template <typename C>
    static std::string component_name() {
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

    // iteration

    template<typename F, typename... C>
    void     for_each(F user_function, bool include_inactive=false);

    template<typename F, typename... C>
    void     for_each(F user_function, bool include_inactive=false) const;

    // systems
    template <typename... P>
    System&              add_system(P&& ...pars);
    
    template <typename S>
    System const&        system() const;

    std::vector<System*> systems() const;

    template <typename S>
    void                 remove_system();

    // globals

    Global&       global();
    Global const& global() const;

    // events

    void           send_event(Event ev);

    template <typename T>
    std::vector<T> event_queue() const;

    void           clear_event_queue();
    
    // debugging

    void           debug_entity(std::ostream& os, EntityOrName const& ent) const;

    template <typename C>
    void           debug_component(std::ostream& os, EntityOrName const& ent) const;

    void           debug_entities(std::ostream& os) const;
    void           debug_systems(std::ostream& os) const;
    void           debug_global(std::ostream& os) const;
    void           debug_event_queue(std::ostream& os) const;

    void           debug_all(std::ostream& os) const;

    // information

    size_t number_of_entities() const;
    size_t number_of_components() const;
    size_t number_of_events() const;
    size_t number_of_systems() const;
    size_t event_queue_size() const;
    size_t memory_used() const;

    // integrity

    void verify_integrity() const;

    // {{{ testable
#ifdef TEST

    template <typename C>
    bool components_are_sorted() const {
        auto& vec1 = comp_vec<C>(true),
              vec2 = comp_vec<C>(false);
        return std::is_sorted(vec1.begin(), vec1.end(), 
                [](auto const& p1, auto const& p2) { return p1.first < p2.first; })
            && std::is_sorted(vec2.begin(), vec2.end(), 
                [](auto const& p1, auto const& p2) { return p1.first < p2.first; });
    }

#endif
    // }}}

private:
    // {{{ templates
    
    // create a tuple from the component list

    using ComponentTupleVector = typename std::tuple<std::vector<std::pair<Entity, Components>>...>;
    static_assert(std::tuple_size<ComponentTupleVector>::value > 0, "Add at least one component.");

    // }}}
    
    // {{{ private
    
    Entity entity(EntityOrName const& ent) const {
        // {{{ ...
        if (auto s = std::get_if<std::string>(&ent)) {
            try {
                return _named_entities.at(*s);
            } catch (std::out_of_range&) {
                throw ECSError(std::string("Entity '") + *s + "' was not found.");
            }
        } else {
            Entity entity = std::get<Entity>(ent);
            if (_entities.find(entity) == _entities.end())
                throw ECSError(std::string("Entity ") + std::to_string(entity.get()) + " was not found.");
            return entity;
        }
        // }}}
    }

    template <typename C>
    std::vector<std::pair<Entity, C>>& comp_vec(bool active) {
        return std::get<std::vector<std::pair<Entity, C>>>(active ? _components_active : _components_inactive);
    }

    template <typename C>
    std::vector<std::pair<Entity, C>> const& comp_vec(bool active) const {
        return std::get<std::vector<std::pair<Entity, C>>>(active ? _components_active : _components_inactive);
    }

    std::map<Entity, bool>        _entities            = {};
    ComponentTupleVector          _components_active   = {},
                                  _components_inactive = {};
    std::map<std::string, Entity> _named_entities      = {};
    std::map<Entity, std::string> _debugging_info      = {};

    size_t                        _next_entity_id      = 0;

    // }}}

};

// {{{ inline implementation

// 
// ENTITIES
//

#define TEMPLATE template <typename System, typename Global, typename Event, typename... Components>
#define ENGINE Engine<System, Global, Event, Components...>

TEMPLATE Entity
ENGINE::add_entity(std::optional<std::string> const& name)
{
    Entity ent { _next_entity_id++ };
    _entities[ent] = true;

    if (name)
        _named_entities.insert_or_assign(name.value(), ent);

    return ent;
}

TEMPLATE bool
ENGINE::is_entity_active(EntityOrName const& ent) const
{
    return _entities.at(entity(ent));
}

TEMPLATE void
ENGINE::set_entity_active(EntityOrName const& ent, bool active)
{
    _entities.at(entity(ent)) = active;

    // TODO - move between component list
}

TEMPLATE std::optional<std::string>
ENGINE::entity_debugging_info(EntityOrName const& ent) const
{
    auto it = _debugging_info.find(entity(ent));
    if (it == end(_debugging_info))
        return {};
    else
        return it->second;
}

TEMPLATE void
ENGINE::set_entity_debugging_info(EntityOrName const& ent, std::string const& text)
{
    _debugging_info[entity(ent)] = text;
}

TEMPLATE void
ENGINE::remove_entity(EntityOrName const& ent)
{
    Entity id = entity(ent);

    // remove from _entities
    _entities.erase(id);

    // remove from _components
    auto remove_component = [&](auto& t) {
        t.erase(std::remove_if(t.begin(), t.end(), [&](auto const& p) { return p.first == id; }), t.end());
    };
    (remove_component(comp_vec<Components>(true)), ...);
    (remove_component(comp_vec<Components>(false)), ...);

    // remove from _named_entities
    for (auto it = _named_entities.begin(); it != _named_entities.end(); ) {
        if (it->second == id)
            _named_entities.erase(it++);
        else
            ++it;
    }

    // remove from _debugging_info
    _debugging_info.erase(id);
}

// 
// INFO
//

TEMPLATE size_t
ENGINE::number_of_entities() const 
{
    return _entities.size();
    /*
    std::vector<size_t> counts;
    (counts.push_back(std::get<std::vector<std::pair<Entity, Components>>>(_components).size()), ...);
    return *std::max_element(begin(counts), end(counts));
    */
}

#undef TEMPLATE
#undef ENGINE

// }}}

}

#if 0  // {{{   old implementation

#include <algorithm>
#include <csetjmp>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#ifndef NOABI
#  include <cxxabi.h>
#endif

#ifdef GTEST
#  include <gtest/gtest_prod.h>
#endif

namespace ECS {

// {{{ EXCEPTIONS

class ECSError : public std::runtime_error {
public:
    explicit ECSError(std::string const& what_arg) : runtime_error(what_arg) {}
    explicit ECSError(const char* what_arg)        : runtime_error(what_arg) {}
};

// }}}

using NoQueue = std::variant<int>;
using NoSystem = void;
struct NoGlobal {};

template<
    typename System,
    typename Global,
    typename Event,
    typename... Components>
class Engine {
public:
    ~Engine() {
        // call destructor for each component of each entity
        _rd.for_each_entity([&](size_t, uint8_t* entity_ptr) {
            _rd.for_each_component_in_entity(entity_ptr, [&](typename decltype(_rd)::Component* c, uint8_t* data, entity_size_t) {
                _destructors[c->id](data);
                return false;
            });
            return false;
        });
        // clear systems
        for(auto it = _systems.rbegin(); it != _systems.rend(); ++it) { delete *it; }
    }

    // 
    // ENTITIES
    //
public:
    size_t add_entity() {
        return _rd.add_entity(); 
    }

    size_t add_entity(std::string const& id) {
        size_t n = add_entity();
        _entities[id] = n;
        return n;
    }

    size_t entity(std::string const& id) const {
        auto it = _entities.find(id);
        if (it == _entities.end())
            throw ECSError("Entity id '" + id + "' not found.");
        return it->second;
    }

    std::string const& entity_name(size_t id) const {
        for (auto const& [k, v] : _entities)
            if (v == id)
                return k;
        throw ECSError("Entity id " + std::to_string(id) + " not found.");
    }

    void remove_entity(size_t entity) {
        // call destructor for each component
        _rd.for_each_component_in_entity(_rd.entity_ptr(entity), [&](typename decltype(_rd)::Component* c, uint8_t* data, entity_size_t) {
            _destructors[c->id](data);
            return false;
        });
        // remove from entity list
        for (auto it = std::begin(_entities); it != std::end(_entities); ) {
            if (it->second == entity)
                _entities.erase(it++);
            else
                ++it;
        }
        // invalidate entity
        _rd.invalidate_entity(entity);
    }

    void remove_entity(std::string const& id) {
        remove_entity(entity(id));
    }

private:
    std::map<std::string, size_t> _entities {};

    //
    // COMPONENTS
    //
private:
    template<typename C>
    C* allocate_component(size_t ent) {
        if(has_component<C>(ent)) {
            throw ECSError("Component already exists in entity.");
        }
        component_id_t id = component_id<C>();
        // allocate space for the component, then initialize it
        return reinterpret_cast<C*>(_rd.add_empty_component(ent, sizeof(C), id));
    }

public:
    template <typename C>
    void add_component(size_t ent, C c) {
        new(allocate_component<C>(ent)) C(std::move(c));
    }

    template <typename C>
    void add_component(std::string const& ent, C c) {
        new(allocate_component<C>(ent)) C(std::move(c));
    }

    template<typename C, typename... P> 
    void add_component(size_t ent, P&& ...pars) {
        new(allocate_component<C>(ent)) C(pars...);
    }

    template<typename C, typename... P> 
    void add_component(std::string const& ent, P&& ...pars) {
        new(allocate_component<C>(entity(ent))) C(pars...);
    }

    template<typename C>
    C const& component(size_t ent) const {
        C const* component = component_ptr<C>(ent);
        if(component == nullptr) {
            throw ECSError("Entity does not contain this component.");
        }
        return *component;
    }

    template<typename C>
    C const& component(std::string const& ent) const {
        return component<C>(entity(ent));
    }

    template<typename C>
    C& component(size_t ent) {
        return const_cast<C&>(static_cast<const Engine<System, Global, Event, Components...>*>(this)->component<C>(ent));
    }

    template<typename C>
    C& component(std::string const& ent) {
        return component<C>(entity(ent));
    }

    template<typename C>
    C const* component_ptr(size_t ent) const {
        void const* cdata = nullptr;
        _rd.for_each_component_in_entity(_rd.entity_ptr(ent), [&](typename decltype(_rd)::Component const* c, uint8_t const* data, entity_size_t) {
            if(c->id == component_id<C>()) {
                cdata = data;
                return true;
            }
            return false;
        });
        if(!cdata) {
            return nullptr;
        }
        return reinterpret_cast<C const*>(cdata);
    }

    template<typename C>
    C const* component_ptr(std::string const& ent) const {
        return component_ptr<C>(entity(ent));
    }

    template<typename C>
    C* component_ptr(size_t ent) {
        return const_cast<C*>(static_cast<const Engine<System, Global, Event, Components...>*>(this)->component_ptr<C>(ent));
    }

    template<typename C>
    C* component_ptr(std::string const& ent) {
        return component_ptr<C>(entity(ent));
    }

    template<typename C>
    bool has_component(size_t ent) const
    {
        bool has = false;
        _rd.for_each_component_in_entity(_rd.entity_ptr(ent), [&](typename decltype(_rd)::Component const* c, uint8_t const*, entity_size_t) {
            if(c->id == component_id<C>()) {
                has = true;
                return true;
            }
            return false;
        });
        return has;
    }

    template<typename C>
    bool has_component(std::string const& ent) const
    {
        return has_component<C>(entity(ent));
    }

    template<typename C>
    void remove_component(size_t ent) {
        _rd.invalidate_component(ent, component_id<C>(), [](void* data) { 
            reinterpret_cast<C*>(data)->~C();
        });
    }

    template<typename C>
    void remove_component(std::string const& ent) {
        return remove_component(entity(ent));
    }

    // 
    // MANAGEMENT
    //
    void compress() { 
        _rd.compress(); 
    }

    //
    // ITERATION
    //
    template<typename... C, typename F>
    void for_each(F const& user_function) const {

        _rd.for_each_entity([&](size_t entity, uint8_t const* entity_ptr) {

            // Here, we prepare for a longjmp. If the component C is not found
            // when for_each_parameter is called, it calls longjmp, which skips
            // calling the user function.
            jmp_buf env_buffer;
            int val = setjmp(env_buffer);

            // Call the user function. `val` is 0 when the component is found.
            if(val == 0) {
                user_function(entity, for_each_parameter<C>(entity_ptr, env_buffer)...);
            }

            return false;
        });
    }

    template<typename... C, typename F>
    void for_each(F const& user_function) {

        _rd.for_each_entity([&](size_t entity, uint8_t* entity_ptr) {

            // Here, we prepare for a longjmp. If the component C is not found
            // when for_each_parameter is called, it calls longjmp, which skips
            // calling the user function.
            jmp_buf env_buffer;
            int val = setjmp(env_buffer);

            // Call the user function. `val` is 0 when the component is found.
            if(val == 0) {
                user_function(entity, for_each_parameter<C>(entity_ptr, env_buffer)...);
            }

            return false;
        });
    }

private:
    template<typename C> C const& for_each_parameter(uint8_t const* entity_ptr, jmp_buf env_buffer) const {
        void const* cdata = nullptr;
        _rd.for_each_component_in_entity(entity_ptr, [&](typename decltype(_rd)::Component const* c, uint8_t const* data, entity_size_t) {
            if(c->id == component_id<C>()) {
                cdata = data;
                return true;
            }
            return false;
        });
        if(!cdata) {
            longjmp(env_buffer, 1);
        }
        return *reinterpret_cast<C const*>(cdata);
    }

    template<typename C> C& for_each_parameter(uint8_t const* entity_ptr, jmp_buf env_buffer) {
        return const_cast<C&>(static_cast<const Engine<System, Global, Event, Components...>*>(this)->for_each_parameter<C>(entity_ptr, env_buffer));
    }
public:

    //
    // SYSTEMS
    //
    template<typename S, typename... P> S& add_system(P&& ...pars) {
        _systems.push_back(new S(pars...));
        return *static_cast<S*>(_systems.back());
    }

    template<typename S> S const& system() const {
        for(auto& sys: _systems) {
            S const* s = dynamic_cast<S const*>(sys);
            if(s) {
                return *s;
            }
        }
        throw std::runtime_error("System not found.");
    }

    std::vector<System*> const& systems() const { return _systems; }

    //
    // QUEUE
    //
public:
    void send(Event ev) { _events.push_back(std::move(ev)); }

    template <typename T>
    std::vector<T> events() const {
        std::vector<T> r;
        for (auto ev : _events)
            if (std::holds_alternative<T>(ev))
                r.push_back(std::get<T>(ev));
        return r;
    }

    void clear_queue() { _events.clear(); }

private:
    std::vector<Event> _events {};

    //
    // GLOBALS
    //
public:
    Global&       global() { return _global; }
    Global const& global() const { return _global; }

private:
    Global _global {};

    //
    // DEBUGGING
    //
private:
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

public:
    void examine(std::ostream& os, size_t entity = std::numeric_limits<size_t>::max()) const {
        auto examine_entity = [&](size_t entity, uint8_t const* entity_ptr) {
            os << "  '" << entity << "': {\n";
            bool first = true;
            for (auto const& [name, id] : _entities) {
                if (id == entity) {
                    if (first) {
                        os << "    'names' : [ ";
                        first = false;
                    }
                    os << "'" << name << "', ";
                }
            }
            if (!first)
                os << "]\n";
            _rd.for_each_component_in_entity(entity_ptr, [&](typename decltype(_rd)::Component const* c, uint8_t const* data, entity_size_t) {
                os << "    ";
                _debuggers[c->id](os, data);
                os << ",\n";
                return false;
            });
            os << "  },\n";
            return false;
        };
        os << "{\n";
        if(entity == std::numeric_limits<size_t>::max()) {
            examine_global(os);
            _rd.for_each_entity(examine_entity);
        } else {
            examine_entity(entity, _rd.entity_ptr(entity));
        }
        os << "}\n";
    }

    void examine(std::ostream& os, std::string const& ent) const {
        examine(os, entity(ent));
    }

    // execute operator<< of global (with and without)
    template<typename T=Global, typename std::enable_if<has_ostream_method<T>::value>::type* = nullptr>
    void examine_global(std::ostream& os) const {
        os << "  'global': {\n    " << global() << "\n  },\n";
    }

    template<typename T=Global, typename std::enable_if<!has_ostream_method<T>::value>::type* = nullptr>
    void examine_global(std::ostream& os) const {
        (void) os;
    }

#ifndef GTEST
private:
#endif
    // {{{ RAW DATA INTERFACE

    template<typename entity_size_t, typename component_id_t, typename component_size_t>
    class RawData {

        static_assert(std::is_signed<entity_size_t>::value, "Entity size type must be signed.");
        static_assert(std::is_unsigned<component_id_t>::value, "Component ID type must be unsigned.");
        static_assert(std::is_unsigned<component_size_t>::value, "Component size type must be signed.");

    public:
        RawData() {
            // _ary.reserve(64 * 1024);
        }

        struct Entity {
            entity_size_t sz;
            void*         data;
        };

        struct Component {
            component_size_t sz;
            component_id_t   id;
        };

        static constexpr size_t         INVALIDATED_ENTITY    = std::numeric_limits<size_t>::max();
        static constexpr component_id_t INVALIDATED_COMPONENT = std::numeric_limits<component_id_t>::max();

        size_t add_entity() {
            // insert index in _entities
            _entities.push_back(_ary.size());
            // insert size in _ary
            entity_size_t sz = static_cast<entity_size_t>(sizeof(entity_size_t));
            _ary_append_bytes(&sz, sz, -1);
            return _entities.size()-1;
        }

        bool is_entity_valid(size_t entity) const {
            return entity_size(entity) >= 0;
        }

        entity_size_t entity_size(size_t entity) const {
            return *reinterpret_cast<entity_size_t const*>(&_ary[_entities[entity]]);
        }

        uint8_t const* entity_ptr(size_t entity) const {
            if(entity > _entities.size()) {
                throw ECSError("Entity does not exist.");
            }
            size_t idx = _entities[entity];
            if(idx == INVALIDATED_ENTITY) {
                throw ECSError("Entity was removed.");
            }
            return &_ary[idx];
        }

        uint8_t* entity_ptr(size_t entity) {
            return const_cast<uint8_t*>(static_cast<const Engine<System, Global, Event, Components...>::RawData<entity_size_t, component_id_t, component_size_t>*>(this)->entity_ptr(entity));
        }

        void invalidate_entity(size_t entity) {
            // IMPORTANT: this doesn't call the destructor for the entities
            entity_size_t* entity_sz = reinterpret_cast<entity_size_t*>(&_ary[_entities[entity]]);
            memset(&_ary[_entities[entity] + sizeof(entity_size_t)], 0xFF, *entity_sz - sizeof(entity_size_t));
            *entity_sz = static_cast<entity_size_t>(-(*entity_sz));
            _entities[entity] = INVALIDATED_ENTITY;
        }

        void* add_empty_component(size_t entity, component_size_t sz, component_id_t id) {
            // create component
            Component component { sz, id };
            size_t idx = _find_space_for_component(entity, sizeof(Component) + sz);
            _ary_copy_bytes(&component, sizeof(Component), idx);
            return &_ary[idx + sizeof(Component)];
        }

        void add_component(size_t entity, component_size_t sz, component_id_t id, void* data) {
            memcpy(add_empty_component(entity, sz, id), data, sz);
        }

        template<typename F>
        void for_each_entity(F const& f, bool skip_invalid = true) {
            if(_ary.empty()) {
                return;
            }
            size_t entity = 0;
            uint8_t *entity_ptr = &_ary[0],
                    *entity_end = &_ary[0] + _ary.size();
            while(entity_ptr < entity_end) {
                entity_size_t entity_sz = *reinterpret_cast<entity_size_t*>(entity_ptr);
                
                if((skip_invalid && entity_sz >= 0) || !skip_invalid) {
                    bool stop = f(entity, entity_ptr);
                    if(stop) {
                        return;
                    }
                }
                
                entity_ptr += abs(entity_sz);
                ++entity;
            }
        }

        template<typename F>
        void for_each_entity(F const& f, bool skip_invalid = true) const {
            if(_ary.empty()) {
                return;
            }
            size_t entity = 0;
            uint8_t const* entity_ptr = &_ary[0],
                    *entity_end = &_ary[0] + _ary.size();
            while(entity_ptr < entity_end) {
                entity_size_t entity_sz = *reinterpret_cast<entity_size_t const*>(entity_ptr);
                
                if((skip_invalid && entity_sz >= 0) || !skip_invalid) {
                    bool stop = f(entity, entity_ptr);
                    if(stop) {
                        return;
                    }
                }
                
                entity_ptr += abs(entity_sz);
                ++entity;
            }
        }

        // TODO - const version
        template<typename F>
        void for_each_component_in_entity(uint8_t* entity_ptr, F const& f, bool skip_invalid = true) {
            entity_size_t entity_sz = *reinterpret_cast<entity_size_t*>(entity_ptr);
            if(entity_sz < 0) {
                throw ECSError("Using a removed entity");
            }
            uint8_t* initial_entity_ptr = entity_ptr;
            uint8_t* end = entity_ptr + entity_sz;
            entity_ptr += sizeof(entity_size_t);
            while(entity_ptr < end) {
                Component* component = reinterpret_cast<Component*>(entity_ptr);
                if((skip_invalid && component->id != INVALIDATED_COMPONENT) || !skip_invalid) {
                    if(f(component, entity_ptr + sizeof(Component), static_cast<entity_size_t>(entity_ptr - initial_entity_ptr))) {
                        return;
                    }
                }
                entity_ptr += component->sz + sizeof(Component);
            }
        }

        template<typename F>
        void for_each_component_in_entity(uint8_t const* entity_ptr, F const& f, bool skip_invalid = true) const {
            entity_size_t entity_sz = *reinterpret_cast<entity_size_t const*>(entity_ptr);
            if(entity_sz < 0) {
                throw ECSError("Using a removed entity");
            }
            uint8_t const* initial_entity_ptr = entity_ptr;
            uint8_t const* end = entity_ptr + entity_sz;
            entity_ptr += sizeof(entity_size_t);
            while(entity_ptr < end) {
                Component const* component = reinterpret_cast<Component const*>(entity_ptr);
                if((skip_invalid && component->id != INVALIDATED_COMPONENT) || !skip_invalid) {
                    if(f(component, entity_ptr + sizeof(Component), static_cast<entity_size_t>(entity_ptr - initial_entity_ptr))) {
                        return;
                    }
                }
                entity_ptr += component->sz + sizeof(Component);
            }
        }

        template<typename F>
        void invalidate_component(size_t entity, component_id_t id, F const& destructor) {
            uint8_t* entity_ptr = &_ary[_entities[entity]];

            entity_size_t entity_sz = *reinterpret_cast<entity_size_t*>(entity_ptr);
            if(entity_sz < 0) {
                throw ECSError("Using a removed entity");
            }

            bool found = false;
            for_each_component_in_entity(entity_ptr, [&](Component* component, void* data, entity_size_t) {
                if(id == component->id) {
                    destructor(data);
                    component->id = INVALIDATED_COMPONENT;
                    found = true;
                    return true;  // stop searching
                }
                return false;
            });

            if(!found) {
                throw ECSError("No such component to remove.");
            }
        }

        void compress() {
            std::vector<uint8_t> newary = {};
            newary.reserve(_ary.size());   // avoids multiple resizing - we shrink it later

            for_each_entity([&](size_t entity, uint8_t* entity_ptr) {
                entity_size_t entity_sz = *reinterpret_cast<entity_size_t*>(entity_ptr);
                size_t current_newary = newary.size();
                // if entity is not invalidated, add it to the new ary
                if(entity_sz >= 0) {
                    entity_size_t current_sz = sizeof(entity_size_t);
                    newary.insert(end(newary), sizeof(entity_size_t), 0);  // placeholder
                    // if component is not invalidated, add it to the new ary
                    for_each_component_in_entity(entity_ptr, [&](Component* component, void* data, entity_size_t) {
                        if(component->id != INVALIDATED_COMPONENT) {
                            size_t sz = newary.size();
                            newary.insert(end(newary), sizeof(Component) + component->sz, 0);
                            memcpy(&newary[sz], component, sizeof(Component));
                            memcpy(&newary[sz + sizeof(Component)], data, component->sz);
                            current_sz = static_cast<entity_size_t>(current_sz + sizeof(Component) + component->sz);
                        }
                        return false;
                    });
                    // adjust entity size
                    entity_size_t* entity_sz_ptr = reinterpret_cast<entity_size_t*>(&newary[current_newary]);
                    *entity_sz_ptr = current_sz;
                    // adjust _entities
                    _entities[entity] = current_newary;
                }
                return false;
            }, false);

            newary.shrink_to_fit();
            _ary = move(newary);
        }

    private:
#ifdef GTEST
        FRIEND_TEST(RawTest, add_entity);
        FRIEND_TEST(RawTest, add_components);
        FRIEND_TEST(RawTest, invalidate_components);
        FRIEND_TEST(RawTest, InvalidateEntities);
        FRIEND_TEST(RawTest, compress);
        FRIEND_TEST(RawTest, DifferentSizes);
        FRIEND_TEST(RawTest, InvalidSizes);
        FRIEND_TEST(RawTest, IterateConst);

        FRIEND_TEST(RawTestData, vector_data);
        FRIEND_TEST(RawTestStr, add_entities_and_components);

        FRIEND_TEST(EngineLeakTest, Leak);
#endif

        std::vector<size_t> _entities = {};
        std::vector<uint8_t> _ary = {};

        template<typename PTR>
        void _ary_append_bytes(PTR* origin, size_t sz, ssize_t pos) {
            if(pos == -1) {
                pos = _ary.size();
            }
            _ary.insert(begin(_ary)+pos, sz, 0);    // IMPORTANT! _ary might relocate here!
            _ary_copy_bytes(origin, sz, pos);
        }

        template<typename PTR>
        void _ary_copy_bytes(PTR* origin, size_t sz, size_t pos) {
            memcpy(&_ary[pos], reinterpret_cast<uint8_t*>(origin), sz);
        }

        size_t _find_space_for_component(size_t entity, size_t total_sz) {
            // find entity size
            if(entity >= _entities.size()) {
                throw ECSError("Entity does not exist");
            }
            size_t idx = _entities[entity];
            if(idx == INVALIDATED_ENTITY) {
                throw ECSError("Using a removed entity");
            }
            entity_size_t entity_sz = *reinterpret_cast<entity_size_t*>(&_ary[idx]);
            
            // check if we can reuse any inactive components
            uint8_t* entity_ptr = &_ary[idx];
            entity_size_t pos = -1;
            for_each_component_in_entity(entity_ptr, [&](Component* component, void*, entity_size_t offset) -> bool {
                if(component->id == INVALIDATED_COMPONENT && component->sz <= entity_sz) {
                    pos = offset;
                    return true;
                }
                return false;
            }, false);
            if(pos != -1) {
                return idx + static_cast<size_t>(pos);
            }

            // we can't reuse inactive components, so we open space in _ary
            if(entity_sz + total_sz > std::numeric_limits<entity_size_t>::max()) {
                throw ECSError("By adding this component, the entity would become too large.");
            }
            _ary.insert(begin(_ary) + idx + entity_sz, total_sz, 0);  // IMPORTANT! _ary can relocate here!
            // adjust indexes
            for(auto it=begin(_entities)+entity+1; it != end(_entities); ++it) {
                *it += total_sz;
            }
            // adjust entity size
            entity_size_t* sz = reinterpret_cast<entity_size_t*>(&_ary[idx]);
            *sz = static_cast<entity_size_t>(entity_sz + total_sz);
            return idx + static_cast<size_t>(entity_sz);
        }
    };

    // }}}
/*
class RawData<entity_size_t, component_id_t, component_size_t> {
    void          add_entity();
    bool          is_entity_valid(size_t entity) const;
    entity_size_t entity_size(size_t entity) const;
    uint8_t*      entity_ptr(size_t) const;
    void          invalidate_entity(size_t entity);
  
    void          add_component(size_t entity, component_size_t sz,
                               component_id_t id, void* data);
    void          invalidate_component(size_t entity, component_id_t id,
                                      function<void(void* data)> destructor);
  
    // return true to stop searching
    void          for_each_entity(function<bool(size_t entity, uint8_t* entity_ptr)> f, 
                                bool skip_invalid = true);
    void          for_each_component_in_entity(uint8_t* entity_ptr, 
                      function<bool(Component* c, uint8_t* data, entity_size_t pos)> f);
  
    void          compress();
}
*/

    // {{{ TEMPLATE MAGIC
    
    // "function" that returns a signed integer type based on the number
    template<size_t n, typename = void> struct SignedDataTypeImpl;
    template<size_t n> struct SignedDataTypeImpl<n, typename std::enable_if<(n <= std::numeric_limits<int8_t>::max())>::type> { using type = int8_t; };
    template<size_t n> struct SignedDataTypeImpl<n, typename std::enable_if<(n > std::numeric_limits<int8_t>::max() && n <= std::numeric_limits<int16_t>::max())>::type> { using type = int16_t; };
    template<size_t n> struct SignedDataTypeImpl<n, typename std::enable_if<(n > std::numeric_limits<int16_t>::max() && n <= std::numeric_limits<int32_t>::max())>::type> { using type = int32_t; };
    template<size_t n> struct SignedDataTypeImpl<n, typename std::enable_if<(n > std::numeric_limits<int32_t>::max() && n <= std::numeric_limits<int64_t>::max())>::type> { using type = int64_t; };
    template<size_t n> using SignedDataType = typename SignedDataTypeImpl<n>::type;

    // "function" that returns a unsigned integer type based on the number
    template<size_t n, typename = void> struct UnsignedDataTypeImpl;
    template<size_t n> struct UnsignedDataTypeImpl<n, typename std::enable_if<(n <= std::numeric_limits<uint8_t>::max())>::type> { using type = uint8_t; };
    template<size_t n> struct UnsignedDataTypeImpl<n, typename std::enable_if<(n > std::numeric_limits<uint8_t>::max() && n <= std::numeric_limits<uint16_t>::max())>::type> { using type = uint16_t; };
    template<size_t n> struct UnsignedDataTypeImpl<n, typename std::enable_if<(n > std::numeric_limits<uint16_t>::max() && n <= std::numeric_limits<uint32_t>::max())>::type> { using type = uint32_t; };
    template<size_t n> struct UnsignedDataTypeImpl<n, typename std::enable_if<(n > std::numeric_limits<uint32_t>::max() && n <= std::numeric_limits<uint64_t>::max())>::type> { using type = uint64_t; };
    template<size_t n> using UnsignedDataType = typename UnsignedDataTypeImpl<n>::type;

    // return the maximum size of a list of types
    template<typename T> static constexpr size_t max_size() { return sizeof(T); }
    template<typename T, typename U, typename... V> static constexpr size_t max_size() {
        return max_size<T>() > max_size<U, V...>() ?  max_size<T>() : max_size<U, V...>();
    }

    // return the sum of the sizes of a list of types
    template<typename T> static constexpr size_t sum_size() { return sizeof(T); }
    template<typename T, typename U, typename... V> static constexpr size_t sum_size() {
        return sum_size<T>() + sum_size<U, V...>();
    }

    // create a tuple from the component list
    using ComponentTuple = typename std::tuple<Components...>;
    static_assert(std::tuple_size<ComponentTuple>::value > 0, "Add at least one component.");

    // detect types
    using entity_size_t    = SignedDataType<sum_size<Components...>()>;                  // entity index size
    using component_id_t   = UnsignedDataType<std::tuple_size<ComponentTuple>::value>;   // component id size
    using component_size_t = UnsignedDataType<max_size<Components...>()>;                // component index size

    // find index by type in a tuple
    template<typename T, typename Tuple> struct tuple_index;
    template<typename T, typename... Types> struct tuple_index<T, std::tuple<T, Types...>> { static const size_t value = 0; };
    template<typename T, typename U, typename... Types> struct tuple_index<T, std::tuple<U, Types...>> { static const size_t value = 1 + tuple_index<T, std::tuple<Types...>>::value; };

    // return the ID of a component
    template<typename C> static constexpr component_id_t component_id() {
        return tuple_index<C, ComponentTuple>::value;
    }
    
    // execute operator<< of struct (with and without)
    template<typename C, typename std::enable_if<has_ostream_method<C>::value>::type* = nullptr>
    std::function<void(std::ostream&, void const*)> create_debugger() {
        return [](std::ostream& os, void const* data) {
            C const* c = reinterpret_cast<C const*>(data);

            int status;
            std::string tname = typeid(C).name();
#ifndef NOABI
            char *demangled_name = abi::__cxa_demangle(tname.c_str(), nullptr, nullptr, &status);
            if(status == 0) {
                tname = demangled_name;
                free(demangled_name);
            }
#endif

            os << "'" << tname << "': { " << *c << " }";
        };
    }

    template<typename C, typename std::enable_if<!has_ostream_method<C>::value>::type* = nullptr>
    std::function<void(std::ostream&, void const*)> create_debugger() {
        return [](std::ostream& os, void const*) {
            int status;
            std::string tname = typeid(C).name();
#ifndef NOABI
            char *demangled_name = abi::__cxa_demangle(tname.c_str(), nullptr, nullptr, &status);
            if(status == 0) {
                tname = demangled_name;
                free(demangled_name);
            }
#endif
            os << "'" << tname << "': {}";
        };
    }

    // }}}

    template<typename C>
    std::function<void(void*)> create_destructor() {
        return [](void* data) {
            reinterpret_cast<C*>(data)->~C();
        };
    }

    RawData<entity_size_t, component_id_t, component_size_t> _rd = {};
    std::vector<std::function<void(void*)>> _destructors = { create_destructor<Components>()... };
    std::vector<std::function<void(std::ostream&, void const*)>> _debuggers = { create_debugger<Components>()... };
    std::vector<size_t> _sizes = { sizeof(Components)... };
    std::vector<System*> _systems = {};
};


}  // namespace ECS

#endif  // #if 0  // }}}

#endif

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
