#ifndef FASTECS_HH_
#define FASTECS_HH_

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <type_traits>
#include <tuple>
#include <stdexcept>
#include <sstream>
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
using  NoEventQueue = std::variant<std::nullptr_t>;

struct Entity {
    size_t get()                          { return value; }
    // {{{ ...
    explicit Entity(size_t value)           : value(value) {}
    explicit Entity(Entity&& entity)        : value(std::move(entity.value)) {}
    size_t   get() const                    { return value; }

    Entity(Entity const& e)                 : value(e.get()) {}
    Entity& operator=(Entity const& e)      { value = e.value; return *this; }

    bool operator==(Entity const& e) const  { return value == e.get(); }
    bool operator!=(Entity const& e) const  { return value != e.get(); }
    bool operator<(Entity const& e) const   { return value < e.get(); }

private:
    size_t   value;
    // }}}
};

const Entity InvalidEntity = Entity(std::numeric_limits<size_t>::max());

using EntityOrName = std::variant<Entity, std::string>;

template <typename System, typename Global, typename Event, typename... Components>
class Engine {
public:

    template <typename... P>
    explicit Engine(P&& ...pars) : _global(Global { pars... }) {}

    //
    // entities
    //

    Entity                     add_entity(std::optional<std::string> const& name = {});

    Entity                     entity(EntityOrName const& ent) const {
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

    bool                       is_entity_active(EntityOrName const& ent) const;
    void                       set_entity_active(EntityOrName const& ent, bool active);

    std::optional<std::string> entity_debugging_info(EntityOrName const& ent) const;
    void                       set_entity_debugging_info(EntityOrName const& ent, std::string const& text);

    void                       remove_entity(EntityOrName const& ent);

    //
    // components
    //

    template <typename C>
    C&       add_component(EntityOrName const& ent, C&& c) {
        // {{{ ...
        Entity id = entity(ent);
        auto& vec = comp_vec<C>(_entities.at(id));
        auto it = std::lower_bound(begin(vec), end(vec), id,
            [](auto const& p, Entity const& e) { return p.first < e; });

        if (it != vec.end() && it->first == id)
            throw ECSError(std::string("Component '") + type_name<C>() + "' already exist for entity " + std::to_string(id.get()) + ".");
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
            throw ECSError(std::string("Entity ") + std::to_string(entity(ent).get()) + " has no component '" + type_name<C>() + "'.");
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
            throw ECSError(std::string("Entity ") + std::to_string(id.get()) + " has no component '" + type_name<C>() + "'.");
        // }}}
    }

    //
    // iteration
    //

    // {{{ type aliases
    template <typename C>
    using my_iter = typename std::vector<std::pair<Entity, C>>::iterator;

    template <typename C>
    using my_citer = typename std::vector<std::pair<Entity, C>>::const_iterator;
    // }}}

    template<typename... C, typename F>
    void     for_each(F user_function, bool include_inactive=false) {
        // {{{ ...

        auto iteration = [&](bool active) {
            // initialize a tuple of iterators, each one pointing to the initial iterator of its component vector
            std::tuple<my_iter<C>...> current;
            ((std::get<my_iter<C>>(current) = comp_vec<C>(active).begin()), ...);
            
            // while none of the iterators reached end
            while (((std::get<my_iter<C>>(current) != comp_vec<C>(active).end()) || ...)) {
                // find iterator that is more advanced
                std::vector<Entity> entities1 { std::get<my_iter<C>>(current)->first... };
                Entity last = *std::max_element(entities1.begin(), entities1.end());

                // advance all iterators that are behind the latest one
                (((std::get<my_iter<C>>(current)->first < last) ? std::get<my_iter<C>>(current)++ : std::get<my_iter<C>>(current)), ...);
                if (((std::get<my_iter<C>>(current) == comp_vec<C>(active).end()) || ...))
                    break;

                // if all iterators are equal, call user function and advance all iterators
                std::vector<Entity> entities2 { std::get<my_iter<C>>(current)->first... };
                if (std::adjacent_find(entities2.begin(), entities2.end(), std::not_equal_to<Entity>()) == entities2.end()) {
                    user_function(*this, entities2.at(0), std::get<my_iter<C>>(current)->second...);
                    (std::get<my_iter<C>>(current)++, ...);
                }
            }
        };

        iteration(true);
        if (include_inactive)
            iteration(false);

        // }}}
    }

    template<typename... C, typename F>
    void     for_each(F user_function, bool include_inactive=false) const {
        // {{{ ...

        auto iteration = [&](bool active) {
            // initialize a tuple of iterators, each one pointing to the initial iterator of its component vector
            std::tuple<my_citer<C>...> current;
            ((std::get<my_citer<C>>(current) = comp_vec<C>(active).cbegin()), ...);
            
            // while none of the iterators reached end
            while (((std::get<my_citer<C>>(current) != comp_vec<C>(active).cend()) || ...)) {
                // find iterator that is more advanced
                std::vector<Entity> entities1 { std::get<my_citer<C>>(current)->first... };
                Entity last = *std::max_element(entities1.cbegin(), entities1.cend());

                // advance all iterators that are behind the latest one
                (((std::get<my_citer<C>>(current)->first < last) ? std::get<my_citer<C>>(current)++ : std::get<my_citer<C>>(current)), ...);
                if (((std::get<my_citer<C>>(current) == comp_vec<C>(active).cend()) || ...))
                    break;

                // if all iterators are equal, call user function and advance all iterators
                std::vector<Entity> entities2 { std::get<my_citer<C>>(current)->first... };
                if (std::adjacent_find(entities2.cbegin(), entities2.cend(), std::not_equal_to<Entity>()) == entities2.cend()) {
                    user_function(*this, entities2.at(0), std::get<my_citer<C>>(current)->second...);
                    (std::get<my_citer<C>>(current)++, ...);
                }
            }
        };

        iteration(true);
        if (include_inactive)
            iteration(false);

        // }}}
    }

    //
    // systems
    //

    template<typename S, typename... P> S&      add_system(P&& ...pars) {
        // {{{ ...
        for(auto& sys: _systems) {
            S const* s = dynamic_cast<S const*>(sys.get());
            if(s)
                throw ECSError("A system of this type already exist in system list.");
        }
        _systems.push_back(std::make_shared<S>(pars...));
        return *static_cast<S*>(_systems.back().get());
        // }}}
    }

    template<typename S> S const&               system() const {
        // {{{
        for(auto& sys: _systems) {
            S const* s = dynamic_cast<S const*>(sys.get());
            if(s) {
                return *s;
            }
        }
        throw std::runtime_error("System not found.");
        // }}}
    }

    std::vector<std::shared_ptr<System>> const& systems() const { return _systems; }

    template <typename S>
    void                                        remove_system() {
        // {{{
        for (auto it = _systems.begin(); it != _systems.end(); ) {
            S const* s = dynamic_cast<S const*>(it->get());
            if (s)
                it = _systems.erase(it);
            else
                ++it;
        }
        
        // }}}
    }

    // 
    // globals
    //

    Global&       global()                  { return _global; }
    Global const& global() const            { return _global; }

    //
    // events
    //


    void           send_event(Event&& ev)   { _events.push_back(std::move(ev)); }

    template <typename T>
    std::vector<T> event_queue() const {
        // {{{ ...
        std::vector<T> r;
        for (auto ev : _events)
            if (std::holds_alternative<T>(ev))
                r.push_back(std::get<T>(ev));
        return r;
        // }}}
    }

    void           clear_queue()            { _events.clear(); }
    
    //
    // debugging
    //

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

    template <typename C>
    std::string debug_component(EntityOrName const& ent) const { return "{ " + debug_object<C>(component<C>(ent)) + "}"; }

    std::string debug_entity(EntityOrName const& ent, size_t spaces=0) const;
    std::string debug_entities(bool include_inactive=false, size_t spaces=0) const;
    std::string debug_global() const;

    std::string debug_all(bool include_inactive=false) const;

    //
    // information
    //

    size_t number_of_entities() const;
    size_t number_of_components() const;
    size_t number_of_event_types() const;
    size_t number_of_systems() const;
    size_t event_queue_size() const;

    // {{{ testable
#ifdef TEST

    template <typename C>
    bool components_are_sorted() const {
        auto& vec1 = comp_vec<C>(true);
        auto& vec2 = comp_vec<C>(false);
        return std::is_sorted(vec1.begin(), vec1.end(), 
                [](auto const& p1, auto const& p2) { return p1.first < p2.first; })
            && std::is_sorted(vec2.begin(), vec2.end(), 
                [](auto const& p1, auto const& p2) { return p1.first < p2.first; });
    }

#endif
    // }}}

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
    
    // {{{ private methods
    
    template <typename C>
    std::vector<std::pair<Entity, C>>& comp_vec(bool active) {
        return std::get<std::vector<std::pair<Entity, C>>>(active ? _components_active : _components_inactive);
    }

    template <typename C>
    std::vector<std::pair<Entity, C>> const& comp_vec(bool active) const {
        return std::get<std::vector<std::pair<Entity, C>>>(active ? _components_active : _components_inactive);
    }

    template <typename C>
    void move_component(Entity const& id, bool from, bool to) {
        // {{{ ...
        auto& origin = comp_vec<C>(from);
        auto& dest   = comp_vec<C>(to);

        // origin iterator
        auto it_origin = std::lower_bound(begin(origin), end(origin), id,
            [](auto const& p, Entity const& e) { return p.first < e; });
        if (it_origin == origin.end() || it_origin->first != id)  // not found
            return;
        
        // dest iterator
        auto it_dest = std::lower_bound(begin(dest), end(dest), id,
            [](auto const& p, Entity const& e) { return p.first < e; });
        if (it_dest != dest.end() && it_dest->first == id)  // shouldn't happen
            throw std::logic_error(std::string("Component '") + type_name<C>() + "' already exist for entity " + std::to_string(id.get()) + ". This is an internal ecs error.");

        dest.insert(it_dest, std::move(*it_origin));
        origin.erase(it_origin);
        // }}}
    }

    template <typename Obj>
    std::string debug_object(Obj const& obj) const {
        // {{{ ...
        // TODO - use ['...'] when there is an invalid character
        std::stringstream ss;
        ss << type_name<Obj>() << " = {";
        if constexpr(has_ostream_method<Obj>::value)
            ss << " " << obj << " ";
        ss << "}, ";
        return ss.str();
        // }}}
    }

    // }}}

    // {{{ private members

    Global                               _global              {};

    std::vector<std::shared_ptr<System>> _systems             {};
    std::vector<Event>                   _events              {};

    std::map<Entity, bool>               _entities            {};
    ComponentTupleVector                 _components_active   {},
                                         _components_inactive {};
    std::map<std::string, Entity>        _named_entities      {};
    std::map<Entity, std::string>        _debugging_info      {};

    size_t                               _next_entity_id      = 0;

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
    Entity id = entity(ent);
    _entities.at(id) = active;

    if (active)
        (move_component<Components>(id, false, true), ...);
    else
        (move_component<Components>(id, true, false), ...);
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
// DEBUGGING
//
TEMPLATE std::string
ENGINE::debug_entity(EntityOrName const& ent, size_t spaces) const
{
    std::string s = std::string(spaces, ' ') + "{ ";
    ((s += has_component<Components>(ent) ? debug_object<Components>(component<Components>(ent)) : ""), ...);
    return s + "}";
}

TEMPLATE std::string
ENGINE::debug_entities(bool include_inactive, size_t spaces) const
{
    std::vector<Entity> ents;
    for (auto const& [ent, active] : _entities) {
        if (include_inactive || active)
            ents.push_back(ent);
    }
    std::sort(ents.begin(), ents.end());

    std::string s = "{\n";
    for (auto const& ent : ents) {
        auto it = _debugging_info.find(ent);
        if (it != _debugging_info.end())
            s += std::string(spaces, ' ') + std::string("   -- ") + it->second + "\n";
        s += std::string(spaces, ' ') + std::string("   [");
        for (auto const& [name, entity] : _named_entities) {
            if (entity == ent) {
                s += "{" + std::to_string(ent.get()) + ", '" + name + "'}";
                goto skip;
            }
        }
        s += std::to_string(ent.get());
skip:
        s += "] = " + debug_entity(ent) + ",\n";
    }
    return s + std::string(spaces, ' ') + "}";
}

TEMPLATE std::string
ENGINE::debug_global() const
{
    std::stringstream ss;
    ss << "{ " << global() << " }";
    return ss.str();
}

TEMPLATE std::string
ENGINE::debug_all(bool include_inactive) const
{
    return std::string("{\n   global = ") + debug_global() + ",\n"
        "   entities = " + debug_entities(include_inactive, 3) + "\n}";
}


// 
// INFO
//

TEMPLATE size_t
ENGINE::number_of_entities() const 
{
    return _entities.size();
}

TEMPLATE size_t
ENGINE::number_of_components() const
{
    return std::tuple_size<ComponentTupleVector>::value;
}

TEMPLATE size_t
ENGINE::number_of_event_types() const
{
    if constexpr (std::is_same<Event, NoEventQueue>::value)
        return 0;
    return std::variant_size_v<Event>;
}

TEMPLATE size_t
ENGINE::number_of_systems() const
{
    return _systems.size();
}

TEMPLATE size_t
ENGINE::event_queue_size() const
{
    return _events.size();
}

#undef TEMPLATE
#undef ENGINE

// }}}

}

#endif

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
