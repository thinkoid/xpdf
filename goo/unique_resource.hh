// -*- mode: c++; -*-

//
// Original code (http://tinyurl.com/hpzgalw) is copyright Eric Niebler and
// Peter Sommerlad, (c) 2016 to present.
//

#ifndef STD_UNIQUE_RESOURCE_HH
#define STD_UNIQUE_RESOURCE_HH

#include <limits>
#include <memory>
#include <utility>

namespace std {
namespace detail {

template< typename T >
constexpr auto should_move_assign_v =
    std::is_nothrow_move_assignable_v< T > || !std::is_copy_assignable_v< T >;

template< typename T >
using move_assign_result = std::conditional< should_move_assign_v< T >, T&&, const T& >;

template< typename T >
using move_assign_result_t = typename move_assign_result< T >::type;

template< typename T >
inline constexpr move_assign_result_t< T >
move_assign_cast (T& x) noexcept {
    return std::move (x);
}

template< typename T >
inline constexpr T& as_const (T& x) noexcept {
    return x;
}

template< bool B > inline void rethrow_helper () { throw; }
template< > inline void rethrow_helper< true > () { }

////////////////////////////////////////////////////////////////////////

struct scope_ignore;

template< typename T >
struct box {
    box (T const& t) noexcept (noexcept (T (t)))
        : value (t)
        { }

    box (T&& t) noexcept (noexcept (T (std::move_if_noexcept (t))))
        : value (std::move_if_noexcept (t))
        { }

private:
    template< typename U >
    using enable_constructor = std::enable_if_t< std::is_constructible_v< T, U > >;

public:
    template< typename U, typename G = scope_ignore,
              typename = enable_constructor< U > >
    explicit box (U&& u, G&& guard = G ()) noexcept (
        noexcept (T (std::forward< U > (u))))
        : value (std::forward< U > (u)) {
        guard.release ();
    }

    T& get () noexcept {
        return value;
    }

    const T& get () const noexcept {
        return value;
    }

    T&& move () noexcept {
        return std::move (value);
    }

private:
    T value;
};

template< typename T >
struct box< T& > {
    template< typename U >
    using enable_constructor = std::enable_if_t< std::is_convertible_v< U, T& > >;

public:
    template< typename U, typename G = scope_ignore,
              typename = enable_constructor< U > >
    box (U&& u, G&& guard = G ()) noexcept (noexcept (static_cast< T& > (u)))
        : value (static_cast< T& > (u)) {
        guard.release ();
    }

    T& get () const noexcept {
        return value.get ();
    }

    T& move () const noexcept {
        return get ();
    }

private:
    std::reference_wrapper< T > value;
};

struct scope_ignore {
    void release () noexcept { }
};

struct scope_exit_policy {
    bool value = true;

    void release () noexcept {
        value = false;
    }

    bool should_execute () noexcept {
        return value;
    }
};

struct scope_fail_policy {
    int value = std::uncaught_exceptions ();

    void release () noexcept {
        value = std::numeric_limits< int >::max ();
    }

    bool should_execute () noexcept {
        return value < std::uncaught_exceptions ();
    }
};

struct scope_success_policy {
    int value = std::uncaught_exceptions ();

    void release () noexcept {
        value = -1;
    }

    bool should_execute () noexcept {
        return value >= std::uncaught_exceptions ();
    }
};

////////////////////////////////////////////////////////////////////////

template< typename, typename = scope_exit_policy >
struct scope_guard;

template< typename T >
using scope_exit = scope_guard< T, scope_exit_policy >;

template< typename T >
using scope_fail = scope_guard< T, scope_fail_policy >;

template< typename T >
using scope_success = scope_guard< T, scope_success_policy >;

template< bool value >
using boolean_constant = std::integral_constant< bool, value >;

template< typename F, typename P >
struct scope_guard : P {
private:
    template< typename T >
    static auto make_guard (std::true_type, T) {
        return scope_ignore { };
    }

    template< typename T >
    static auto make_guard (std::false_type, T* pf) {
        return scope_guard< T&, P > (*pf);
    }

private:
    template< typename FP >
    using is_constructible_from =
        std::is_constructible< box< F >, FP, scope_ignore >;

    template< typename FP >
    static constexpr auto is_constructible_from_v =
        is_constructible_from< FP >::value;

    template< typename FP >
    using is_nothrow_constructible_from = boolean_constant<
        noexcept (box< F > (std::declval< FP >(), P { })) >;

    template< typename FP >
    static constexpr auto is_nothrow_constructible_from_v =
        is_nothrow_constructible_from< FP >::value;

public:
    template< typename FP,
              typename = std::enable_if_t< is_constructible_from_v< FP > > >
    explicit scope_guard (FP&& p)
        noexcept (is_nothrow_constructible_from_v< FP >)
        : function_ ((FP&&)p, scope_guard::make_guard (
                         is_nothrow_constructible_from< FP > { }, &p))
        { }

    scope_guard (scope_guard&& other)
        noexcept(noexcept (box< F > (other.function_.move (), other)))
        : P (other), function_ (other.function_.move (), other)
        { }

    scope_guard& operator= (scope_guard &&) = delete;

    ~scope_guard () noexcept (
        noexcept (std::declval< box< F >& > ().get ()())) {
        if (P::should_execute ())
            function_.get ()();
    }

    scope_guard (const scope_guard&) = delete;
    scope_guard& operator=(const scope_guard&) = delete;

private:
    box< F > function_;
};

template< typename F, typename P >
void swap (scope_guard< F, P >&, scope_guard< F, P >&) = delete;

////////////////////////////////////////////////////////////////////////

template< typename T >
constexpr auto is_reference_v = std::is_reference< T >::value;

template< typename T >
constexpr auto is_nothrow_move_constructible_v =
    std::is_nothrow_move_constructible< T >::value;

template< typename T >
constexpr auto is_copy_constructible_v =
    std::is_copy_constructible< T >::value;

template< typename T, typename U >
constexpr auto is_nothrow_constructible_v =
    std::is_nothrow_constructible< T, U >::value;

template< typename F, typename P >
inline auto make_scope_guard (F&& f) {
    return scope_guard< std::decay_t< F >, P > (std::forward< F > (f));
}

} // namespace detail

////////////////////////////////////////////////////////////////////////

template< typename F >
auto make_scope_exit (F&& f)
    noexcept (detail::is_nothrow_constructible_v< std::decay_t< F >, F >) {
    return detail::make_scope_guard<
        F, detail::scope_exit_policy > (std::forward< F > (f));
}

template< typename F >
auto make_scope_fail (F&& f)
    noexcept (detail::is_nothrow_constructible_v< std::decay_t< F >, F >) {
    return detail::make_scope_guard<
        F, detail::scope_fail_policy > (std::forward< F > (f));
}

template< typename F >
auto make_scope_success (F&& f)
    noexcept (detail::is_nothrow_constructible_v< std::decay_t< F >, F >) {
    return detail::make_scope_guard<
        F, detail::scope_success_policy > (std::forward< F > (f));
}

////////////////////////////////////////////////////////////////////////

template< typename T >
struct null_delete {
    constexpr null_delete() noexcept = default;

    template< typename U >
    null_delete (const null_delete< U >&) noexcept { }

    template< typename U >
    void operator() (const U&) const { }
};

////////////////////////////////////////////////////////////////////////

template< typename R, typename D >
struct unique_resource {
private:
    static_assert (
        detail::is_nothrow_move_constructible_v< R > ||
        detail::is_copy_constructible_v< R >,
        "resource must be nothrow_move_constructible or copy_constructible");

    static_assert (
        detail::is_nothrow_move_constructible_v< D > ||
        detail::is_copy_constructible_v< D >,
        "deleter must be nothrow_move_constructible or copy_constructible");

private:
    // More helpers
    template< typename T >
    static constexpr auto is_boxable_resource_v = std::is_constructible_v<
        detail::box< R >, T, detail::scope_ignore >;

    template< typename T >
    static constexpr auto is_boxable_deleter_v = std::is_constructible_v<
        detail::box< D >, T, detail::scope_ignore >;

    template< typename T, typename U >
    using enable_member_t = std::enable_if_t<
        is_boxable_resource_v< T > && is_boxable_deleter_v< U > >;

private:
    template< typename, typename >
    friend struct unique_resource;

    detail::box< R > resource_;
    detail::box< D > deleter_;

    bool execute_on_reset_{ true };

private:
    unique_resource (const unique_resource&) = delete;
    unique_resource& operator= (const unique_resource&) = delete;

public:
    template< typename T, typename U, typename = enable_member_t< T, U > >
    explicit unique_resource (T&& t, U&& u, bool b)
        noexcept (
            noexcept (detail::box< R > ((R&&)t, detail::scope_ignore { })) &&
            noexcept (detail::box< D > ((D&&)u, detail::scope_ignore { })))
        : resource_ (std::forward< T > (t), make_scope_exit ([&] { if (b) u (t); })),
          deleter_  (std::forward< U > (u), make_scope_exit ([&, this] { if (b) u (get ()); })),
          execute_on_reset_ (b)
        { }

    template< typename T, typename U, typename = enable_member_t< T, U > >
    explicit unique_resource (T&& t, U&& u)
        noexcept (
            noexcept (detail::box< R > (forward< T > (t), detail::scope_ignore { })) &&
            noexcept (detail::box< D > (forward< U > (u), detail::scope_ignore { })))
        : resource_ (std::forward< T > (t), make_scope_exit ([&] { u (t); })),
          deleter_  (std::forward< U > (u), make_scope_exit ([&, this] { u (get ()); }))
        { }

    template< typename T, typename U, typename = enable_member_t< T, U > >
    unique_resource (unique_resource< T, U >&& other)
        noexcept (
            noexcept (detail::box< R > (other.resource_.move (), detail::scope_ignore { })) &&
            noexcept (detail::box< D > (other.deleter_.move (),  detail::scope_ignore { })))
        : resource_ (other.resource_.move (), detail::scope_ignore { }),
          deleter_  (other. deleter_.move (), make_scope_exit (
              [&, this] () noexcept {
                  other.get_deleter ()(get ());
                  other.release ();
              })),
          execute_on_reset_ (std::exchange (other.execute_on_reset_, false))
        { }

    template< typename T, typename U, typename = enable_member_t< T, U > >
    unique_resource& operator= (unique_resource< T, U >&& other)
        noexcept (is_nothrow_move_assignable_v< R > &&
                  is_nothrow_move_assignable_v< D >) {
        if (this != &other) {
            reset ();

            if (is_nothrow_move_assignable_v< detail::box< R > >) {
                deleter_  = detail::move_assign_cast (other.deleter_);
                resource_ = detail::move_assign_cast (other.resource_);
                execute_on_reset_ = std::exchange (other.execute_on_reset_, false);
            }
            else if (is_nothrow_move_assignable_v< detail::box< D > >) {
                resource_ = detail::move_assign_cast (other.resource_);
                deleter_  = detail::move_assign_cast (other.deleter_);
                execute_on_reset_ = std::exchange (other.execute_on_reset_, false);
            }
            else {
                resource_ = detail::as_const (other.resource_);

                //
                // Try to avoid a resource leak, the resource has been assigned
                // successfully, and the old deleter has lost track of it:
                //
                try {
                    deleter_  = detail::as_const (other.deleter_);
                    execute_on_reset_ = std::exchange (other.execute_on_reset_, false);
                }
                catch (...) {
                    //
                    // Release the resource with the old deleter:
                    //
                    other.get_deleter ()(get ());

                    //
                    // Deactivate all deleters:
                    //
                    execute_on_reset_ = other.execute_on_reset_ = false;

                    //
                    // And re-throw matching the function type:
                    //
                    detail::rethrow_helper<
                        is_nothrow_move_assignable_v< R > &&
                        is_nothrow_move_assignable_v< D > > ();
                }
            }
        }

        return *this;
    }

    ~unique_resource () noexcept {
        reset ();
    }

    void
    reset () noexcept {
        if (execute_on_reset_) {
            execute_on_reset_ = false;
            get_deleter ()(get ());
        }
    }

    void
    reset (R&& r) noexcept (noexcept (detail::box< R > (std::move (r)))) {
        reset ();
        resource_ = std::move (r);
        execute_on_reset_ = true;
    }

    const R& release () noexcept {
        execute_on_reset_ = false;
        return get ();
    }

    R& get () noexcept {
        return resource_.get ();
    }

    const R& get () const noexcept {
        return resource_.get ();
    }

    operator const R& () const noexcept {
        return get ();
    }

    R operator-> () const noexcept {
        return get ();
    }

    typename std::add_lvalue_reference <
        typename std::remove_pointer< R >::type >::type
    operator* () const {
        return *resource_;
    }

    D& get_deleter () noexcept {
        return deleter_.get ();
    }

    const D& get_deleter () const noexcept {
        return deleter_.get ();
    }
};

template< typename T, typename U >
auto make_unique_resource (T&& t, U&& u) noexcept (
    noexcept (unique_resource< T, U > (
                  std::forward< T > (t),
                  std::forward< U > (u)))) {
    return unique_resource< T, U > (
        std::forward< T > (t),
        std::forward< U > (u));
}

template< class T, class U, class S = std::decay_t< T > >
unique_resource< std::decay_t< T >, std::decay_t< U > >
make_unique_resource_checked (T&& t, const S& s, U&& u)
    noexcept (is_nothrow_constructible_v< std::decay_t< T >, T > &&
              is_nothrow_constructible_v< std::decay_t< U >, U >) {
    bool b = t != s;
    return unique_resource< std::decay_t< T >, std::decay_t< U > >{
        std::forward< T > (t), std::forward< U > (u), b };
}

}

#endif // STD_UNIQUE_RESOURCE_HH
