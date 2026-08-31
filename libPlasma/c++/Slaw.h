
/* (c)  oblong industries */

#ifndef OBLONG_PLASMA_SLAW_H
#define OBLONG_PLASMA_SLAW_H


#include <libLoam/c/ob-api.h>

#include "CompositeSlaw.h"
#include "SlawRef.h"
#include "slaw-traits.h"
#include "PlasmaxxRetorts.h"

#include "libPlasma/c/slaw-ordering.h"


/* Slaw tries to keep two different representations, CompositeSlaw and
 * SlawRef, in sync with each other.  However, it is buggy and sometimes
 * the illusion breaks down, and there is different behavior depending
 * on which representation is used.  See bugs 2432, 10495, and 10833. */


namespace oblong {
namespace plasma {


class SlawIterator;


/**
 * Wrapper around C-style @e slawx, managing memory life-cycle and
 * providing easy access to the value(s) wrapped by a slaw.
 *
 * This is the main entry point of externally provided slaw values into
 * C++ land. By wrapping such a value in a Slaw instance, you let @e
 * Plasma++ take care of its memory management, and gain ways of
 * extracting values stored by the slaw, including other slawx inside
 * composites.
 *
 * The class can also be used the other way round: as a factory of
 * C-level slaw values. Slaw provides a constructor for every C type
 * wrappable in a C-level slaw, and a set of (static) factory
 * functions for composite slawx (maps, lists, and the like).
 *
 * All these goodies are provided with value semantics. Slaw instances
 * are immutable, and most of the time passed by value (or const
 * reference, if you really can't afford copying two pointers: see
 * below). Copy and assignment are provided, and as a result, Slaw is a
 * good class for the contained type in containers relying on equality.
 *
 * Note that copying Slaw instances is cheap: being immutable, the
 * underlying C-slaw is @e not copied, but ref-counted. Composite slawx
 * are not freed until the last reference to any of its constitutive
 * slaw disappears, so you're free to mix and match (parts of) Slaw at
 * low cost.
 *
 * Slawx abstract over their value type: as their C counterparts, they
 * can represent atomic built-in types (integers, floats, and so on),
 * arrays thereof, simple structured vectors (v2int32, v3float64, ...)
 * and composite slawx (conses, lists, maps and proteins). Thus, Slaw's
 * interface provides member functions to access the wrapped value and
 * its components. The @c Emit/Into family of functions deals with
 * atomic, array and structured types and Slaw conversion to the
 * corresponding C++ values--for all of them, we have a default 'null'
 * value (T (0)), when the underlying slaw is not of the requested type.
 *
 * In the same vein, we provide member functions to access the innards
 * of composite slaw (e.g., Nth or Slice for lists, Find for maps and
 * IndexOf for lists). The question here is what to do when you use a
 * member function meant for, say, lists on a Slaw which was not created
 * as a list. We've chosen to push the dynamic character of Slawx one
 * step further: any Slaw can be 'reinterpreted' as a given composite
 * type, and the member function at hand applied. Slawx being immutable,
 * such reinterpretation does not affect the message's receiver, and
 * make our functions complete again in a way that, hopefully, makes
 * sense. Thus, atomic types are reinterpreted as lists of length 1 when
 * you use the list sub-interface; or ints arrays as lists of integers.
 * The documentation below for each group of functions details the
 * reinterpretation business for each composite slaw kind.
 *
 * @ingroup PlasmaSlaw
 */
class OB_PLASMAXX_API Slaw
{
 public:
  typedef SlawIterator iterator;
  typedef SlawIterator const_iterator;

  /**
   * Destructor. If this Slaw was constructed using a C-slaw instance,
   * the latter is freed. This will only happen when this Slaw instance
   * is not part of any other Slaw.
   */
  ~Slaw ();

  /**
   * @name Constructing Slaw instances
   * Members to construct Slaw instances, from
   * C-slawx or built-in types.
   */
  //@{
  /**
   * Empty (NULL) slaw. This constitutes a Slaw instance with no real
   * C-level slaw inside. You can rely on @c Slaw().IsNull() always
   * evaluating to @c true. Null Slaw instances are used, for
   * instance, to mark empty results in lookups, or when asking for
   * out-of-bounds components.
   * @sa IsNull
   */
  Slaw ();

  /**
   * Stores a reference to the given slaw, which is not copied. The
   * new Slaw instance takes ownership of @a s, and is in charge of
   * its memory management.
   * @warning Creating more than one Slaw for the same C-level slaw
   * using this constructor will therefore lead to multiple deletions:
   * use Slaw's copy constructor instead, if that's what you need.
   */
  explicit Slaw (slaw s);

  /**
   * Constructs Slawx from C++ types. This template constructor works
   * whenever @c oblong::plasma::make_slaw() is instantiable, and
   * allows easy creation of new slawx using C++ types. Some
   * examples:
   * @code
   *   v2int32 v2 = { 43, -129 };
   *   Slaw sv2 (v2);
   *   v2int32 v22 (sv2.Emit<v2int32> ());
   *   assert (v2 == v22);
   *
   *   Slaw si (int64(2343));
   *   int64 v;
   *   assert (si.Into (v));
   *   assert (v == 2343);
   * @endcode
   * See also the Emit/Into family of member functions.
   */
  template <typename T>
  explicit Slaw (const T &value);

  /**
   * Constructs a Slaw from a numeric array. This template constructor
   * will only work when @a T is a numeric type. Alternatively, Slaw
   * containing numeric arrays can be created from std::vector
   * instances, as in the following example:
   * @code
   *   std::vector<int32> v = ...;
   *   Slaw s (v);
   *   assert (s.Count () == v.size ());
   *   const int32 *svs;
   *   assert (s.Into (svs));
   *   for (size_t i = 0, l = v.size (); i < l; ++i)
   *     assert (svs[i] == v[i]);
   *   Slaw s2 (svs, s.Count ());
   *   assert (s == s2);
   * @endcode
   */
  template <typename T>
  Slaw (const T *values, size_t count);
  //@}

  /**
   * @name Equality tests
   */
  //@{
  /**
   * Slaw instances are compared as per slawx_equal. In addition, two
   * NULL slawx are considered equal; e.g., it's always the case that
   * Slaw () == Slaw ().
   */
  bool operator== (const Slaw &other) const;

  /**
   * Unequality. Returns !(*this == other).
   */
  bool operator!= (const Slaw &other) const;

  /**
   * Less than.
   */
  bool operator< (const Slaw &other) const
  {
    return slaw_semantic_compare (SlawValue (), other.SlawValue ()) < 0;
  }

  /**
   * Greater than.
   */
  bool operator> (const Slaw &other) const
  {
    return slaw_semantic_compare (SlawValue (), other.SlawValue ()) > 0;
  }

  /**
   * Less than or equal.
   */
  bool operator<= (const Slaw &other) const
  {
    return slaw_semantic_compare (SlawValue (), other.SlawValue ()) <= 0;
  }

  /**
   * Greater than or equal.
   */
  bool operator>= (const Slaw &other) const
  {
    return slaw_semantic_compare (SlawValue (), other.SlawValue ()) >= 0;
  }

  /**
   * Return a 64-bit hash code for this Slaw.
   */
  unt64 Hash () const { return slaw_hash (SlawValue ()); }
  //@}

  /**
   * @name Coercions
   */
  //@{
  /**
   * Access to managed slaw. In case you need the C-slaw value managed
   * by this Slaw instance, here's the member giving you access to it
   * as a bslaw: you're not supposed to freely dispose of the
   * associated memory. If this Slaw instance was made out of its
   * constituents, calling this function will cause a new slaw to come
   * into existence; thus, it's generally wise to only call SlawValue
   * when you really need to. (Of course, for a Slaw created out of a
   * C-level slaw, one just gets the latter's value, with no other
   * overhead.)
   */
  bslaw SlawValue () const;

  /**
   * Checked value emission. Checks whether the underlying C-slaw can
   * emit a value of type @a T, and, in that case, puts that emission
   * in @c value. This member function will work with any type @a T
   * providing a slaw_traits specialization, i.e.:
   *   - All built-in numerical types. The underlying slaw must be
   *     numeric or a string parsable as a number. Coercion works by
   *     first getting the value as a 64bit value of the adequate
   *     type (e.g. @c int64 when @c T is int8, or @c float64 when @c
   *     T is float32), and then performing a vanilla static cast to
   *     @c T.
   *   - v-structs (v2int32, v3float64, v4unt8 and so on). The
   *     underlying slaw must be a numerical array, a string parsable
   *     as a comma-separated list of numbers or a composite Slaw whose
   *     components are coercible to numbers; in all three cases, @c
   *     Slaw::Count() must match the requested length.
   *   - Vect or Vect4. The Slaw must be coercible to v3float64 or,
   *     respectively, v4float64.
   *   - Str and const char*. The underlying slaw must be coercible
   *     to const char*.
   * Examples:
   * @code
   *   Slaw s = get_my_slaw ();
   *   float64 v;
   *   if (s.Into (v))  // T is inferred from v's type
   *     {
   *        // use v
   *     }
   *
   *   Slaw list = Slaw::List (42, 12);
   *   v2float32 fv;
   *   assert (list.Into (fv));
   *   assert (list.Nth (0).Emit<float32>() == fv.x);
   *   assert (list.Nth (1).Emit<float32>() == fv.y);
   * @endcode
   * @sa Emit() and CanEmit() for an alternative way of doing the same
   * thing.
   */
  template <typename T>
  bool Into (T &value) const;

  /**
   * Trivial self-emission
   */
  bool Into (Slaw &value) const;

  /**
   * Checks whether the underlying slaw emits values of type @a T,
   * with the coercion rulesof Into(T&). The type being checked must
   * be explicitly provided, as in the following example, where this
   * function is used in conjunction with Emit().
   *
   * @code
   *   Slaw foo (cut_me_some_slaw ());
   *   if (foo.CanEmit<bool> ())
   *    {
   *       bool v = foo.Emit<bool> (); // here we now it's safe.
   *       // ...
   *    }
   * @endcode
   * @sa Into(T&) for a way of checking and emitting all at once.
   * @sa Is() if you want to determine the exact type, without
   * coercion.
   */
  template <typename T>
  bool CanEmit () const;

  /**
   * Unchecked value emission. This member function will trust the
   * caller in that the underlying slaw is able to emit a value of
   * the requested type. If that's not the case, a default @a T value
   * obtained from slaw_traits<T>::Null() is returned (meaning 0 for
   * built-ins, NULL. for strings, and a default-constructed value
   * for v-structs, Vect, Vect4 and Str).
   * @sa Emit for sample code.
   */
  template <typename T>
  T Emit () const;
  //@}

  /**
   * @name Null and Nil Slawx
   */
  //@{
  /**
   * Checks whether we're managing a real C-slaw or just a NULL
   * one (as created, for instance, by the default constructor).
   * @sa Slaw(), IsNil
   */
  bool IsNull () const;

  /**
   * Factory method for a NULL Slaw. NULL Slawx are use to denote the
   * absence of a Slaw, and can be returned by lookup functions such
   * as Find(). When used as maps or lists, NULL Slawx behave as
   * empty containers. For instance, the following code will create
   * a map with a single key/value entry:
   * @code
   *   Slaw k ("key");
   *   Slaw v (42);
   *   Slaw map = Slaw::Null().MapPut (k, v);
   * @endcode
   * Nulls are not allowed as elements of composite Slaw. Therefore,
   * any attempt to add a NULL to a container will result in the
   * the same container, unmodified.
   * @note Other ways of creating NULL Slawx are this class' default
   * constructor, and @c Slaw(NULL). Any two NULL Slaw, however
   * they're created, are equal.
   */
  static const Slaw &Null ();

  /**
   * Checks whether this is a nil slaw, as returned by Nil(). Not to
   * be confused with IsNull(), which see.
   */
  bool IsNil () const;

  /**
   * Alternate spelling of Nil() which can be used in Objective C++.
   */
  static const Slaw &NilSlaw ();

#ifndef __OBJC__
  /**
   * Factory method for @b nil slawx. Note that nil is an actual
   * C-slaw value, different from NULL. Therefore, @c
   * Slaw::Nil().IsNull() evaluates to @c false.
   */
  static const Slaw &Nil () { return NilSlaw (); }
#endif
  //@}

  /**
   * @name Slaw duck-typing
   * The predicates below check the 'slaw-nature' of the C-level
   * instance wrapped by this Slaw. In other works, they tell you
   * what the type of the bslaw returned by SlawValue () will be.
   */
  //@{
  /**
   * Checks whether the underlying Slaw is of the exact type @a T,
   * without any coercion.
   */
  template <typename T>
  bool Is () const;

  /**
   * Checks whether this represents a single value. An atomic Slaw
   * does not represent an array and cannot be split up into other Slawx.
   * (Non-atomic, non-array Slawx are also called @e composite Slawx
   * in this documentation.) Symbolically:
   * @code
   *     IsAtomic () <=> !IsArray () && !IsComposite ()
   * @endcode
   */
  bool IsAtomic () const;

  /**
   * Checks if this Slaw represents a numeric array. If this
   * predicate evaluates to @c true, IsAtomic() will return @c false,
   * as will IsComposite(). That is,
   * @code
   *     IsArray () <=> !IsAtomic () && !IsComposite ()
   * @endcode
   */
  bool IsArray () const;

  /**
   * Checks whether this Slaw can be decomposed into (contains) other
   * Slawx. That is, this predicate is true when the Slaw represents
   * a Cons, List, Map or Protein. Note that, according to this
   * definition, Slawx containing numeric arrays are @e not
   * composite. To put it symbolically,
   * @code
   *     IsComposite () == !IsAtomic () && !IsArray ()
   * @endcode
   */
  bool IsComposite () const;

  /**
   * Checks whether this Slaw is actually a cons. If that's the case,
   * you can access its composite structure with Car () and Cdr ()
   * without any further reinterpretation.
   */
  bool IsCons () const;

  /**
   * Checks whether this Slaw is actually a list. If that's the case,
   * you can access its composite structure with Nth and friends
   * without it being reinterpreted.
   */
  bool IsList () const;

  /**
   * Checks whether this Slaw is actually a map. If that's the case,
   * you can access its composite structure via Find and related
   * members without triggering reinterpretation of its value.
   */
  bool IsMap () const;

  /**
   * Checks whether this Slaw is actually a protein. If that's the
   * case, well, you already know.
   */
  bool IsProtein () const;
  //@}

  /**
   * @name Serialization
   * Bi-directionaly Slaw serialization using strings or files.
   */
  //@{
  /**
   * Slawx can be serialized to files in a format specified
   * by instances of this class and its derived types.
   */
  class OutputFormat
  {
   public:
    virtual ~OutputFormat () {}
    virtual Slaw AsSlaw () const = 0;
    virtual bool IsBinary () const = 0;
  };

  /**
   * Output options for binary format (none, currently).
   */
  class BinaryFormat : public OutputFormat
  {
   public:
    Slaw AsSlaw () const override { return Slaw (); }
    bool IsBinary () const override { return true; }
  };

  /**
   * Output options for YAML.
   */
  class YAMLFormat : public OutputFormat
  {
   public:
    explicit YAMLFormat (bool use_tag_numbers = true,
                         bool emit_directives = true, bool order_maps = true)
        : tags_ (use_tag_numbers), dirs_ (emit_directives), ordm_ (order_maps)
    {
    }

    YAMLFormat &TagNumbers (bool use = true)
    {
      tags_ = use;
      return *this;
    }
    YAMLFormat &Directives (bool use = true)
    {
      dirs_ = use;
      return *this;
    }
    YAMLFormat &OrderedMaps (bool use = true)
    {
      ordm_ = use;
      return *this;
    }

    bool IsBinary () const override { return false; }
    Slaw AsSlaw () const override
    {
      return Slaw::Map ("tag_numbers", tags_, "directives", dirs_,
                        "ordered_maps", ordm_);
    }

   private:
    bool tags_;
    bool dirs_;
    bool ordm_;
  };

  /**
   * Options that can be specified to the "Spew" methods.
   */
  class SpewOptions
  {
   public:
    SpewOptions ();
    explicit SpewOptions (unt32 flags);

    SpewOptions &RelativeOffset (bool use = true);
    SpewOptions &RudeAscii      (bool use = true);
    SpewOptions &EscapeStrings  (bool use = true);

    SpewOptions &AddFlags (unt32 flags);
    SpewOptions &RemoveFlags (unt32 flags);
    SpewOptions &SetFlags (unt32 flags);
    unt32        GetFlags () const;

    SpewOptions &Prefix (const Str& prolo);
    Str          GetPrefix () const;

   private:
    SpewOptions &ChangeFlag (unt32 flag, bool use);

    unt32 flags_;
    Str   prolo_;
  };

  /**
   * Serializes this Slaw to a file with the given @a path, whose old
   * contents is deleted if it already exists. By default, the output
   * is human readable YAML, but you can set @a format to @c
   * BinaryFormat() for a more compact, if opaque, representation.
   * Returns a success indicator; to get additional details in case
   * of failure, pass a non-null @a err value.
   */
  bool ToFile (const Str &path, ObRetort *err = NULL,
               const OutputFormat &format = YAMLFormat ()) const;

  /**
   * Reads the contents of the file with the given @a path and tries
   * to interpret it as either a text or binary serialized slaw. In
   * case of error, returns a NULL Slaw, and stores an error code in
   * @a err (when provided).
   */
  static Slaw FromFile (const Str &path, ObRetort *err = NULL);

  /**
   * Returns a text representation of this Slaw, or the empty string
   * if some error occurs, whose code will be stored in @a err, if
   * not NULL. If you are reusing the string and prefer a more
   * space-efficient function, you might use ToStringSlaw(): ToString
   * is equivalent to @c ToStringSlaw(err).Emit<Str>().
   */
  Str ToString (ObRetort *err = NULL,
                const YAMLFormat &format = YAMLFormat ()) const;

  /**
   * Works like ToString(), but returns a stringy slaw instead of a
   * Str instance.
   */
  Slaw ToStringSlaw (ObRetort *err = NULL,
                     const YAMLFormat &format = YAMLFormat ()) const;

  /**
   * Creates a new Slaw from the textual representation stored in the
   * given string (which could've been created, for instance, using
   * ToString). In case of error, returns a NULL Slaw, and stores an
   * error code in @a err (when provided).
   */
  static Slaw FromString (const Str &str, ObRetort *err = NULL);

  /**
   * Returns a printable representation of this Slaw, in a format
   * which is reversible only for atomic types. Conses print as
   * "(car, cdr)", lists as "[elem, elem, ...]", maps as
   * "[(key, value), ...]" and proteins as "{descrips, ingests}".
   * If you want detailed debugging info, use Spew() instead.
   * For a fully reversible serialization, see ToString().
   */
  Str ToPrintableString () const;

  /**
   * Debugging aid, dumping a human readable description of this
   * Slaw's contents to the given output stream.
   */
  void Spew (OStreamReference os) const;

  /**
   * Spew to an ostream, with options.
   */
  void Spew (OStreamReference os, const SpewOptions &opts) const;

  /**
   * and hey! how about a spew to olde-tyme unix files:
   */
  void Spew (FILE *ph) const;

  /**
   * Spew to a FILE*, with options.
   */
  void Spew (FILE *ph, const SpewOptions &opts) const;

  /**
   * Or even, because sometimes you're in the debugger and just
   * need this, straight to stderr?
   */
  void SpewToStderr () const;

  /**
   * Spew to a string.
   */
  Str SpewToString () const;

  /**
   * Spew to a string, with options.
   */
  Str SpewToString (const SpewOptions &opts) const;
  //@}

  /**
   * @name Cons interface
   * Member functions accessing cons (pair) members (aka, car and
   * cdr). When applied to a Slaw for which IsCons() is @c false, the
   * following reinterpretation is implied:
   *  - Atomic types with value @c v behave as (v, NULL).
   *  - Arrays are first reinterpreted as a list, and then the
   *    reinterpretation for the latter is used.
   *  - Lists follow the lisp tradition: (Nth (0), Slice (1, Count ()))
   *  - Maps are first viewed as a List of conses, and the
   *    reinterpretation for lists is applied.
   *  - The same mechanism is used for proteins, where the
   *    intermediate list consists of the protein's descrips.
   */
  //@{
  /**
   * Creates a cons Slaw with @a car and @a cdr as its two components.
   * If any of the arguments is null, a null Slaw is returned.
   */
  static Slaw Cons (const Slaw &car, const Slaw &cdr);

  /**
   * Creates a cons Slaw using @a car and @a cdr. The resulting Slaw
   * owns the memory of those two, so you shouldn't reuse them in
   * contexts where they're eventually freed (e.g., to construct
   * other Slawx). As a courtesy to Plasma++ clients, this function
   * takes care of checking whether @a car and @a cdr denote the same
   * slaw, doing the right thing in that case too. However, if any of
   * the arguments is NULL, a null Slaw is returned.
   */
  static Slaw Cons (slaw car, slaw cdr);

  /**
   * Convenience constructor that converts into slawx its two arguments
   * and creates a cons Slaw from them.
   */
  template <typename T, typename U>
  static Slaw Cons (T car, U cdr);

  /**
   * Gets the first Slaw in a pair. If this instance is a Cons deep
   * inside (IsCons() returns @c true), this member gives you access
   * to its first constituent. For lists, you get the first element;
   * other types are first reinterpreted as lists and acted on
   * accordingly afterwards (see this group's documentation).
   */
  Slaw Car () const;

  /**
   * Gets the second Slaw in a pair. For conses this gets the second
   * element in the pair. A list will give you here a new list
   * consisting of all its elements except for the first one. Other
   * Slaw kinds just return a null Slaw.
   */
  Slaw Cdr () const;
  //@}

  /**
   * @name List operations
   * The current Slaw is reinterpreted, or coerced, as a list (if
   * need be) before performing any of the list operations
   * below, with the following semantics:
   *   - NULL is the empty list
   *   - Atomic Slawx are considered single-element lists
   *   - Conses become a two-element list
   *   - Arrays are considered as lists of their elements, converted
   *     to Slaw
   *   - Maps are reinterpreted as a list of key/value conses
   *   - Proteins become their descrip list.
   */
  //@{
  /**
   * Creates a list containing slawx or Slawx enumerated via any input
   * iterator. In particular, you can use an array of slawx, to wit:
   * @code
   *   slaw s[N] = {s1, s2, ..., sN};
   *   Slaw list = Slaw::ListCollect (s, s + N);
   *   assert (list.Count () == N);
   *   for (int i = 0; i < N; ++i)
   *     assert (list[i].SlawValue() == s[i]);
   * @endcode
   * Or one of the standard containers:
   * @code
   *   std::vector<slaw> vsl;
   *   slaw s;
   *   while (s = gimme_a_slaw ()) vsl.push_back (s);
   *   Slaw list = Slaw::ListCollect (list.begin (), list.end ());
   *   assert (list.Count () == vsl.size ());
   *   for (int i = 0; i < N; ++i)
   *     assert (list[i].SlawValue() == vsl[i]);
   * @endcode
   * Null slawx are @b not added to the list:
   * @code
   *   slaw s[3] = {non_null_slaw, NULL, non_null_slaw2};
   *   Slaw lst = Slaw::ListCollect (s, s+3);
   *   assert (lst.Count () == 2);
   *   assert (lst.Nth (0).SlawValue () == non_null_slaw);
   *   assert (lst.Nth (0).SlawValue () == non_null_slaw2);
   * @endcode
   * The new Slaw returned by this function takes ownership of all
   * those little slawx, so you should not reuse them to create any
   * other Slaw instance or, oh the horror, free their memory.
   * Passing the same slaw more than once in a call to this
   * constructor is allowed, though.
   * Sometimes it's useful to have an STL container of slawx.
   * ListCollect will also work with any iterator that yields Slaw
   * instances or slaw. E.g.,
   * @code
   *    ::std::vector slawx;
   *    slawx.push_back (Slaw ("stringy"));
   *    slawx.push_back (Slaw (42));
   *    Slaw list = Slaw::ListCollect (slawx.begin (), slawx.end ());
   *    assert (slawx.size () == list.Count ());
   *    for (int64 i = 0; i < list.Count (); ++i)
   *      assert (slawx[i] == list.Nth (i));
   * @endcode
   */
  template <typename It>
  static Slaw ListCollect (It begin, It end, size_t hint = 10);

  /**
   * Creates a list Slaw containing slawx from an array. This is just
   * an instantiation of ListCollect<It> for @a It a pointer to slaw:
   * see that constructor for additional details. As an example, this
   * call:
   * @code
   *    slaw s[5] = {s0, s1, s2, s3, s4, s5};
   *    Slaw list = Slaw::ListCollect (s, 3);
   * @endcode
   * is equivalent to the iterator version:
   * @code
   *    Slaw list = Slaw::ListCollect (s, s+3);
   * @endcode
   */
  static Slaw ListCollect (slaw *s, unt32 len);

  /**
   * Creates an empty list.
   */
  static Slaw List ();

  /**
   * Creates a list containing the given slawx. Convenience constructor
   * with overloadings taking up to ten slawx and putting them into a
   * list. NULL slawx are skipped; e.g.,
   * @code
   *   Slaw ls = Slaw::List (s0, s1, NULL, s2, s3);
   *   assert (ls.Count () == 4);
   * @endcode
   * Note that you @e don't need to mark the end of the list with a
   * NULL (these are not a varargs function).
   * As is customary, the new SlawList takes ownership of the passed
   * C-level slaw, which must not be used to construct any other Slaw
   * instance. Passing the same slaw more than once in a call to this
   * constructor is allowed, though.
   */
  static Slaw List (slaw);
  /**
   * Creates a list containing two slawx.
   * @sa List(slaw)
   */
  static Slaw List (slaw, slaw);
  /**
   * Creates a list containing three slawx.
   * @sa List(slaw)
   */
  static Slaw List (slaw, slaw, slaw);
  /**
   * Creates a list containing four slawx.
   * @sa List(slaw)
   */
  static Slaw List (slaw, slaw, slaw, slaw);
  /**
   * Creates a list containing five slawx.
   * @sa List(slaw)
   */
  static Slaw List (slaw, slaw, slaw, slaw, slaw);
  /**
   * Creates a list containing six slawx.
   * @sa List(slaw)
   */
  static Slaw List (slaw, slaw, slaw, slaw, slaw, slaw);
  /**
   * Creates a list containing seven slawx.
   * @sa List(slaw)
   */
  static Slaw List (slaw, slaw, slaw, slaw, slaw, slaw, slaw);
  /**
   * Creates a list containing eight slawx.
   * @sa List(slaw)
   */
  static Slaw List (slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw);
  /**
   * Creates a list containing nine slawx.
   * @sa List(slaw)
   */
  static Slaw List (slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw);
  /**
   * Creates a list containing ten slawx.
   * @sa List(slaw)
   */
  static Slaw List (slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw);

  /**
   * Creates a list containing the given Slaw instances. Convenience
   * constructor with overloadings taking up to ten Slawx and putting
   * them into a list. Null Slawx are skipped. Template versions of
   * this constructor are also provided, so that one can create lists
   * using built-in types:
   * @code
   *   Slaw list = Slaw::List (int32(1), "foo", make_v2float64 (3, 2.2));
   *   Slaw list2 = Slaw::List ("bar", make_v2int32 (4, 2));
   * @endcode
   * @sa ListCollect, Prepend or Append if you need
   * to create a list with more than ten Slawx, or with a
   * programatically determined number of them.
   */
  static Slaw List (const Slaw &);
  /**
   * Creates a list of two slawx.
   * @sa List(const Slaw&);
   */
  static Slaw List (const Slaw &, const Slaw &);
  /**
   * Creates a list of three slawx.
   * @sa List(const Slaw&);
   */
  static Slaw List (const Slaw &, const Slaw &, const Slaw &);
  /**
   * Creates a list of four slawx.
   * @sa List(const Slaw&);
   */
  static Slaw List (const Slaw &, const Slaw &, const Slaw &, const Slaw &);
  /**
   * Creates a list of five slawx.
   * @sa List(const Slaw&);
   */
  static Slaw List (const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                    const Slaw &);
  /**
   * Creates a list of six slawx.
   * @sa List(const Slaw&);
   */
  static Slaw List (const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                    const Slaw &, const Slaw &);
  /**
   * Creates a list of seven slawx.
   * @sa List(const Slaw&);
   */
  static Slaw List (const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                    const Slaw &, const Slaw &, const Slaw &);
  /**
   * Creates a list of eight slawx.
   * @sa List(const Slaw&);
   */
  static Slaw List (const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                    const Slaw &, const Slaw &, const Slaw &, const Slaw &);
  /**
   * Creates a list of nine slawx.
   * @sa List(const Slaw&);
   */
  static Slaw List (const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                    const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                    const Slaw &);
  /**
   * Creates a list of ten slawx.
   * @sa List(const Slaw&);
   */
  static Slaw List (const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                    const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                    const Slaw &, const Slaw &);

  /**
   * Convenience template to create lists of one Slaw constructed from
   * a type transformable to Slaw (i.e., a type for which Slaw(T) is
   * defined.
   * @sa List(const Slaw&);
   */
  template <class T0>
  static Slaw List (T0);

  /**
   * Creates a list of two slawx.
   * @sa List(const Slaw&)
   */
  template <class T0, class T1>
  static Slaw List (T0, T1);

  /**
   * Creates a list of three slawx.
   * @sa List(const Slaw&)
   */
  template <class T0, class T1, class T2>
  static Slaw List (T0, T1, T2);

  /**
   * Creates a list of four slawx.
   * @sa List(const Slaw&)
   */
  template <class T0, class T1, class T2, class T3>
  static Slaw List (T0, T1, T2, T3);

  /**
   * Creates a list of five slawx.
   * @sa List(const Slaw&)
   */
  template <class T0, class T1, class T2, class T3, class T4>
  static Slaw List (T0, T1, T2, T3, T4);

  /**
   * Creates a list of six slawx.
   * @sa List(const Slaw&)
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5>
  static Slaw List (T0, T1, T2, T3, T4, T5);

  /**
   * Creates a list of seven slawx.
   * @sa List(const Slaw&)
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5,
            class T6>
  static Slaw List (T0, T1, T2, T3, T4, T5, T6);

  /**
   * Creates a list of eight slawx.
   * @sa List(const Slaw&)
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5,
            class T6, class T7>
  static Slaw List (T0, T1, T2, T3, T4, T5, T6, T7);

  /**
   * Creates a list of nine slawx.
   * @sa List(const Slaw&)
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5,
            class T6, class T7, class T8>
  static Slaw List (T0, T1, T2, T3, T4, T5, T6, T7, T8);

  /**
   * Creates a list of ten slawx.
   * @sa List(const Slaw&)
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5,
            class T6, class T7, class T8, class T9>
  static Slaw List (T0, T1, T2, T3, T4, T5, T6, T7, T8, T9);

  /**
   * Number of Slawx contained in this one. Semantics for different
   * Slaw kinds are as follows:
   *   - NULL: returns 0
   *   - Atomic Slawx: return 1
   *   - Arrays: return the number of values in the array
   *   - Cons: returns 2
   *   - List: returns the number of Slawx in the list
   *   - Map: returns the number of key/value pairs in the map
   *   - Protein: returns length(descrips) + length(ingests)
   */
  int64 Count () const;

  /**
   * Indexed access to the Slawx contained in this one. When
   * positive, @a n is zero-based; passing a value equal or greater
   * than Count() returns a NULL Slaw (see Slaw::IsNull). Negative
   * indexes are legit, @c Nth(-1) being the last element, @c Nth(-2)
   * the one before last and so on: in general, if @c n<0, it gets
   * translated to @c n-Count().
   */
  Slaw Nth (int64 n) const;

  /**
   * Syntactic sugar for Nth, which see.
   */
  Slaw operator[] (int64 n) const;

  /**
   * Creates a new list by inserting the given Slaw into the nth
   * position of the current one (reinterpreted as a list). Negative
   * values of @a n start counting at the end of the list. If @a n is
   * out of bounds, @a s is just prepended or appended to the list,
   * according to @a n's sign. If @a s is a null Slaw, the same list
   * is returned. Inserting a non-null Slaw into a null one produces
   * a list with the latter as only element (i.e., null Slawx behaves
   * as an empty list). Combining those two rules gives us a funny
   * way of creating an empty list: @c Slaw().ListInsert(Slaw(),0).
   */
  Slaw ListInsert (const Slaw &s, int64 n) const OB_WARN_UNUSED_RESULT;

  /**
   * Overload of insert for @c bslaw, creating a new Slaw to be
   * inserted by duplicating @a s. If @a s is NULL, the original
   * Slaw, reinterpreted as a list, is returned.
   */
  Slaw ListInsert (bslaw s, int64 n) const OB_WARN_UNUSED_RESULT;

  /**
   * Convenience version of ListInsert taking a builtin type and
   * creating the inserted Slaw for you.
   */
  template <typename T>
  Slaw ListInsert (T value, int64 n) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new list by appending a given Slaw to this one,
   * reinterpreted as a list. If @a s is null, the original list
   * is returned. Appending a non-null Slaw to a null one creates
   * a singleton list with the former as its only element.
   * @sa ListPrepend, ListInsert, ListConcat
   */
  Slaw ListAppend (const Slaw &s) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new Slaw with the given value, and creates a new list
   * appending it to this Slaw, reinterpreted as a list. Sugar over
   * ListAppend(const Slaw&).
   */
  template <typename T>
  Slaw ListAppend (T s) const OB_WARN_UNUSED_RESULT;

  /**
   * Like ListAppend, but putting @a s in the front of the list.
   * @sa ListAppend ListInsert, ListConcat
   */
  Slaw ListPrepend (const Slaw &s) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new Slaw with the given value, and creates a new list
   * prepending it to this Slaw, reinterpreted as a list. Sugar over
   * ListPrepend(const Slaw&).
   */
  template <typename T>
  Slaw ListPrepend (T s) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new list by interpreting both the current Slaw and @a
   * other as lists and concatenating them into a new one, which is
   * returned.If either (or both) of the involved Slawx is null, it
   * is treated, as customary, as an empty list.
   * @sa ListAppend
   */
  Slaw ListConcat (const Slaw &other) const OB_WARN_UNUSED_RESULT;

  /**
   * Checks whether @a s is part of this Slaw (when the latter is
   * interpreted as a list), using Slaw::operator== as equality test.
   * Returns its index, or an out of bounds negative value if not
   * found. Overloads taking a @c bslaw, @c slaw or built-in type
   * value as parameter are also provided.
   *
   * As usual, this functions work by reinterpreting the target Slaw as
   * a list. See ListFind for a non-coercive search function.
   */
  int64 IndexOf (const Slaw &s, unt64 start = 0) const;
  /**
   * Works like IndexOf(const Slaw &, unt64), but using a bslaw as
   * key.
   */
  int64 IndexOf (bslaw s, unt64 start = 0) const;
  /**
   * Works like IndexOf(const Slaw &, unt64), but using a slaw as
   * key. It does not take ownership of @a s.
   */
  int64 IndexOf (slaw s, unt64 start = 0) const;
  /**
   * Works like IndexOf(T, unt64), with @c start equal to 0.
   */
  template <typename T>
  int64 IndexOf (T value) const;
  /**
   * Works like IndexOf(const Slaw &, unt64), but using a slaw
   * constructed from @a value as the key.
   */
  template <typename T>
  int64 IndexOf (T value, unt64 start) const;

  /**
   * The ListContains family of functions is just a shortcut to the
   * corresponding IndexOf () > -1.
   */
  bool ListContains (const Slaw &s, unt64 start = 0) const;
  /**
   * Equivalent to IndexOf(s,start) > -1.
   */
  bool ListContains (bslaw s, unt64 start = 0) const;
  /**
   * Equivalent to IndexOf(s,start) > -1.
   */
  bool ListContains (slaw s, unt64 start = 0) const;
  /**
   * Equivalent to IndexOf(value) > -1.
   */
  template <typename T>
  bool ListContains (T value) const;
  /**
   * Equivalent to IndexOf(value, start) > -1.
   */
  template <typename T>
  bool ListContains (T value, unt64 start) const;

  /**
   * Checks whether the given Slaw are consecutive constituents of this
   * one. When @a no_gaps is @c true, we're looking for a Slice of this
   * Slaw (which is hence viewed as a list) that equals @a list. If, on
   * the other hand, we accept gaps, we are basically asking that, for
   * every element @c e of @a list, @c IndexOf(e) @c >= @c 0. The
   * function returns the index of the first element in @a list, or -1
   * if the match fails. And yes, if this Slaw is not a list, it is
   * reinterpreted as one.
   */
  int64 IndexOfList (const Slaw &list, bool no_gap = true) const;

  /**
   * Constructs a slice of the current Slaw, reinterpreted (if
   * needed) as a list. The slice contains elements beginning at
   * index @a begin and ending at either @c end-1 or @c Count()-1 if
   * @c end>Count(). If @c end<=begin, an empty list is returned.
   * Negative @a end or @a begin values are transformed to positive
   * values by substracting them from Count().
   */
  Slaw Slice (int64 begin, int64 end) const;

  /**
   * Viewing this Slaw as a list, returns a new list with all
   * ocurrences of @a s removed. If @a s is not found in the list,
   * one gets a copy of this Slaw. Overload taking a @c bslaw
   * provided.
   * @sa ListRemove, ListRemoveNth
   */
  Slaw ListRemove (const Slaw &s) const OB_WARN_UNUSED_RESULT;
  /**
   * @sa ListRemove(const Slaw&)
   */
  Slaw ListRemove (bslaw s) const OB_WARN_UNUSED_RESULT;

  /**
   * Looks for the first occurrence of @a s in this Slaw,
   * reinterpreted as a list, and returns a new list that contains
   * all other elements in the list. If @a s is not found in the
   * list, one gets a copy of this Slaw. Overload taking a @c bslaw
   * provided.
   * @sa ListRemoveAll, ListRemoveNth
   */
  Slaw ListRemoveFirst (const Slaw &s) const OB_WARN_UNUSED_RESULT;
  /**
   * Removes the first occurrence of @a s, considered a Slaw.
   * @sa ListRemoveFirst(const Slaw &)
   */
  Slaw ListRemoveFirst (bslaw s) const OB_WARN_UNUSED_RESULT;

  /**
   * Reinterprets this Slaw as a list and returns a new list
   * containing all its elements but the nth. If @a n is out of
   * bounds, one gets a copy of this Slaw instace.
   * @sa ListRemove, ListRemoveAll
   */
  Slaw ListRemoveNth (int64 n) const OB_WARN_UNUSED_RESULT;

  /**
   * Interpreting this slaw as a list, construct a new one by
   * replacing @a to for @a from. All occurrences of @a from are
   * replaced. Overload taking a @c bslaw as its first argument
   * provided. @a to must be non-null slawx; otherwise, no
   * replacement takes place, even if @a from is a member of this
   * list.
   */
  Slaw ListReplace (const Slaw &from,
                    const Slaw &to) const OB_WARN_UNUSED_RESULT;
  /**
   * @sa ListReplace(const Slaw&, const Slaw&)
   */
  Slaw ListReplace (bslaw from, const Slaw &to) const OB_WARN_UNUSED_RESULT;

  /**
   * Interpreting this slaw as a list, construct a new one by
   * replacing the first occurrence of @a from by @a to. All
   * occurrences of @a from are replaced. Overload taking a @c bslaw
   * as its @a from argument provided. @a to must be non-null slawx
   * for any replacement to take place; otherwise, the target Slaw is
   * returned.
   */
  Slaw ListReplaceFirst (const Slaw &from,
                         const Slaw &to) const OB_WARN_UNUSED_RESULT;
  /**
   * @sa ListReplaceFirst(const Slaw&, const Slaw&)
   */
  Slaw ListReplaceFirst (bslaw from,
                         const Slaw &to) const OB_WARN_UNUSED_RESULT;

  /**
   * Replace the nth element in this Slaw (reinterpreted as a list)
   * by the provided one. The index may be negative (to start
   * counting from the list's tail). If it's out of bounds, a copy of
   * this Slaw is returned. @a s must be non-null slawx;
   * otherwise, a null Slaw is returned.
   */
  Slaw ListReplaceNth (int64 n, const Slaw &s) const OB_WARN_UNUSED_RESULT;

  /**
   * Returns the index of the first occurrence of @a key in the list.
   * Returns -1 if @a key is not found, or this is not a list or map
   * (i.e., no reinterpretation is performed, unlike IndexOf).
   */
  int64 ListFind (bslaw key) const;

  /**
   * Returns the index of the first occurrence of @a key in the list.
   * Returns -1 if @a key is not found, or this is not a list or map.
   * (i.e., no reinterpretation is performed, unlike IndexOf). This
   * function does @b not take ownership of @a key.
   */
  int64 ListFind (slaw key) const;

  /**
   * Returns the index of the first occurrence of @a key in the list.
   * Returns -1 if @a key is not found, or this is not a list or map
   * (i.e., no reinterpretation a la IndexOf).
   */
  int64 ListFind (const Slaw &key) const;

  /**
   * Template version of the lookup function ListFind, so that you can
   * provide the key without constructing a Slaw yourself.
   */
  template <typename T>
  int64 ListFind (T key) const;
  //@}

  /**
   * @name Map interface
   * Conceptually, a map is a list of conses. Thus, all list
   * constructors are also map constructors when you reinterpret
   * their arguments as cons. For each passed Slaw, Car() gives you
   * the key, and Cdr() its value in the new map. Interpretation of a
   * given Slaw as a map follows these rules:
   *   - A map is a map is a map.
   *   - For a list, we have two possibilities:
   *     - If the element at hand is a cons, it's interpreted as a
   *       key/value pair in the map
   *     - Otherwise, the element is skipped.
   *   - A cons is a map with a single entry, with key its car and value
   *     its cdr.
   *   - For all other Slaw kinds, the Slaw is first interpreted as a
   *     list, and then the previous rule is applied.
   *  Thus, we have, for instance:
   *  @code
   *     Slaw list (Slaw::List ("one", "two", Slaw::Cons ("three", 3)));
   *     assert (list.Find ("one").IsNull ());
   *     assert (list.Find ("two").IsNull ());
   *     assert (list.Find ("three") == Slaw (3));
   *     assert (list.Find (3).IsNull());
   *
   *     Slaw cons (Slaw::Cons ("a", "b"));
   *     assert (cons.Find ("a") == Slaw ("b"));
   *  @endcode
   */
  //@{
  /**
   * Creates a map containing slawx enumerated via any input
   * iterator, where keys and values alternate. For instance, one can
   * use an array of slawx:
   * @code
   *   slaw s[N] = {k1, v1, k2, v2, ..., kN, sN};
   *   Slaw map = Slaw::MapCollect (s, s + N);
   *   assert (map.Count () == N / 2); // number of key/value pairs
   *   for (int i = 0; i < N; i += 2)
   *     assert (map.Find (s[i]).SlawValue() == s[i + 1]);
   * @endcode
   * Or one of the standard containers:
   * @code
   *   std::vector<slaw> vsl;
   *   slaw s;
   *   while (s = gimme_a_slaw ()) vsl.push_back (s);
   *   Slaw map = Slaw::MapCollect (list.begin (), list.end ());
   *   assert (map.Count () == vsl.size () / 2);
   *   for (int i = 0; i < N; ++i)
   *     assert (map.Find (s[i]).SlawValue() == s[i + 1]);
   * @endcode
   *
   * The examples above assume that none of the passed slawx is @c
   * NULL, and that @c N is even. Null keys or values are @b not
   * added to the map, and, for N odd, the last key is ignored. E.g.,
   * @code
   *   slaw s[6] = {k1, v1, k2, NULL, k3, v3};
   *   Slaw map = Slaw::MapCollect (s, s + 6);
   *   assert (map.Count () == 2);
   *   assert (map.Find (k1).SlawValue () == v1);
   *   assert (map.Find (k2).IsNull ());
   *   assert (map.Find (k3).SlawValue () == v3);
   * @endcode
   * The new Slaw returned by this function takes ownership of all
   * those little slawx, so you should not reuse them to create any
   * other Slaw instance, let alone freeing their memory. Passing the
   * same slaw more than once in a call to this constructor is
   * allowed, though.
   * MapCollect also works with any other forward iterator type
   * yielding Slaw or slaw instances. E.g.,
   * @code
   *    ::std::vector slawx;
   *    slawx.push_back (Slaw ("key"));
   *    slawx.push_back (Slaw ("value"));
   *    Slaw map = Slaw::MapCollect (slawx.begin (), slawx.end ());
   *    assert (!map.Find ("key").IsNull ());
   * @endcode
   * @note MapCollect produces the same map that would be produced
   * by calling MapPut(), repeatedly, in order.  In other words,
   * if there are duplicate keys, the value of the last duplicate
   * is chosen, but ends up in the position of the first duplicate
   * with that key.
   */
  template <typename It>
  static Slaw MapCollect (It begin, It end, size_t hint = 20);

  /**
   * Creates an empty map.
   */
  static Slaw Map ();

  // clang-format off
  /**
   * Constructor for maps with overridings using up to ten explicitly
   * provided key/value pairs. If either a key or a value is null, the
   * pair is skipped.
   */
  static Slaw Map (const Slaw &k0, const Slaw &v0);
  /**
   * Constructs a map containing two key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (const Slaw &k0, const Slaw &v0,
                   const Slaw &k1, const Slaw &v1);
  /**
   * Constructs a map containing three key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (const Slaw &k0, const Slaw &v0,
                   const Slaw &k1, const Slaw &v1,
                   const Slaw &k2, const Slaw &v2);
  /**
   * Constructs a map containing four key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (const Slaw &k0, const Slaw &v0,
                   const Slaw &k1, const Slaw &v1,
                   const Slaw &k2, const Slaw &v2,
                   const Slaw &k3, const Slaw &v3);
  /**
   * Constructs a map containing five key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (const Slaw &k0, const Slaw &v0,
                   const Slaw &k1, const Slaw &v1,
                   const Slaw &k2, const Slaw &v2,
                   const Slaw &k3, const Slaw &v3,
                   const Slaw &k4, const Slaw &v4);
  /**
   * Constructs a map containing six key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (const Slaw &k0, const Slaw &v0,
                   const Slaw &k1, const Slaw &v1,
                   const Slaw &k2, const Slaw &v2,
                   const Slaw &k3, const Slaw &v3,
                   const Slaw &k4, const Slaw &v4,
                   const Slaw &k5, const Slaw &v5);
  /**
   * Constructs a map containing seven key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (const Slaw &k0, const Slaw &v0,
                   const Slaw &k1, const Slaw &v1,
                   const Slaw &k2, const Slaw &v2,
                   const Slaw &k3, const Slaw &v3,
                   const Slaw &k4, const Slaw &v4,
                   const Slaw &k5, const Slaw &v5,
                   const Slaw &k6, const Slaw &v6);
  /**
   * Constructs a map containing eight key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (const Slaw &k0, const Slaw &v0,
                   const Slaw &k1, const Slaw &v1,
                   const Slaw &k2, const Slaw &v2,
                   const Slaw &k3, const Slaw &v3,
                   const Slaw &k4, const Slaw &v4,
                   const Slaw &k5, const Slaw &v5,
                   const Slaw &k6, const Slaw &v6,
                   const Slaw &k7, const Slaw &v7);
  /**
   * Constructs a map containing nine key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (const Slaw &k0, const Slaw &v0,
                   const Slaw &k1, const Slaw &v1,
                   const Slaw &k2, const Slaw &v2,
                   const Slaw &k3, const Slaw &v3,
                   const Slaw &k4, const Slaw &v4,
                   const Slaw &k5, const Slaw &v5,
                   const Slaw &k6, const Slaw &v6,
                   const Slaw &k7, const Slaw &v7,
                   const Slaw &k8, const Slaw &v8);
  /**
   * Constructs a map containing ten key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (const Slaw &k0, const Slaw &v0,
                   const Slaw &k1, const Slaw &v1,
                   const Slaw &k2, const Slaw &v2,
                   const Slaw &k3, const Slaw &v3,
                   const Slaw &k4, const Slaw &v4,
                   const Slaw &k5, const Slaw &v5,
                   const Slaw &k6, const Slaw &v6,
                   const Slaw &k7, const Slaw &v7,
                   const Slaw &k8, const Slaw &v8,
                   const Slaw &k9, const Slaw &v9);
  // clang-format on

  /**
   * Constructor for maps with overridings using up to ten explicitly
   * provided key/value pairs. All arguments must be non-null slawx;
   * otherwise, the corresponding key/value pair is skipped.
   */
  static Slaw Map (slaw k0, slaw v0);
  /**
   * Constructs a map containing two key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (slaw k0, slaw v0, slaw k1, slaw v1);
  /**
   * Constructs a map containing three key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2);
  /**
   * Constructs a map containing four key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                   slaw k3, slaw v3);
  /**
   * Constructs a map containing five key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                   slaw k3, slaw v3, slaw k4, slaw v4);
  /**
   * Constructs a map containing six key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                   slaw k3, slaw v3, slaw k4, slaw v4, slaw k5, slaw v5);
  /**
   * Constructs a map containing seven key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                   slaw k3, slaw v3, slaw k4, slaw v4, slaw k5, slaw v5,
                   slaw k6, slaw v6);
  /**
   * Constructs a map containing eight key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                   slaw k3, slaw v3, slaw k4, slaw v4, slaw k5, slaw v5,
                   slaw k6, slaw v6, slaw k7, slaw v7);
  /**
   * Constructs a map containing nine key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                   slaw k3, slaw v3, slaw k4, slaw v4, slaw k5, slaw v5,
                   slaw k6, slaw v6, slaw k7, slaw v7, slaw k8, slaw v8);
  /**
   * Constructs a map containing ten key/value entries.  If either a
   * key or a value is null, the corresponding pair is skipped.
   */
  static Slaw Map (slaw k0, slaw v0, slaw k1, slaw v1, slaw k2, slaw v2,
                   slaw k3, slaw v3, slaw k4, slaw v4, slaw k5, slaw v5,
                   slaw k6, slaw v6, slaw k7, slaw v7, slaw k8, slaw v8,
                   slaw k9, slaw v9);

  /**
   * Creates a map with a key/value pair provided explicitly as
   * built-in types.
   */
  template <class T0, class T1>
  static Slaw Map (T0, T1);

  /**
   * Creates a map with 2 key/value pairs provided explicitly as
   * built-in types.
   */
  template <class T0, class T1, class T2, class T3>
  static Slaw Map (T0, T1, T2, T3);

  /**
   * Creates a map with 3 key/value pairs provided explicitly as
   * built-in types.
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5>
  static Slaw Map (T0, T1, T2, T3, T4, T5);

  /**
   * Creates a map with 4 key/value pairs provided explicitly as
   * built-in types.
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5,
            class T6, class T7>
  static Slaw Map (T0, T1, T2, T3, T4, T5, T6, T7);

  /**
   * Creates a map with 5 key/value pairs provided explicitly as
   * built-in types.
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5,
            class T6, class T7, class T8, class T9>
  static Slaw Map (T0, T1, T2, T3, T4, T5, T6, T7, T8, T9);

  /**
   * Creates a map with 6 key/value pairs provided explicitly as
   * built-in types.
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5,
            class T6, class T7, class T8, class T9, class T10, class T11>
  static Slaw Map (T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11);

  /**
   * Creates a map with 7 key/value pairs provided explicitly as
   * built-in types.
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5,
            class T6, class T7, class T8, class T9, class T10, class T11,
            class T12, class T13>
  static Slaw Map (T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13);

  /**
   * Creates a map with 8 key/value pairs provided explicitly as
   * built-in types.
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5,
            class T6, class T7, class T8, class T9, class T10, class T11,
            class T12, class T13, class T14, class T15>
  static Slaw Map (T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                   T14, T15);

  /**
   * Creates a map with 9 key/value pairs provided explicitly as
   * built-in types.
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5,
            class T6, class T7, class T8, class T9, class T10, class T11,
            class T12, class T13, class T14, class T15, class T16, class T17>
  static Slaw Map (T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                   T14, T15, T16, T17);

  /**
   * Creates a map with 10 key/value pairs provided explicitly as
   * built-in types.
   */
  template <class T0, class T1, class T2, class T3, class T4, class T5,
            class T6, class T7, class T8, class T9, class T10, class T11,
            class T12, class T13, class T14, class T15, class T16, class T17,
            class T18, class T19>
  static Slaw Map (T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13,
                   T14, T15, T16, T17, T18, T19);

  /**
   * A list of the keys in this Slaw (viewed as as map).
   */
  Slaw MapKeys () const;

  /**
   * A list of values in this Slaw (viewed as a map).
   */
  Slaw MapValues () const;

  /**
   * Creates a new map by adding the key @c cons.Car() and value @c
   * cons.Cdr() to this one (or assigning the latter to the key if it
   * already exists in this map). If @a cons is a null Slaw, the
   * original Slaw (viewed as a map) is returned. If this Slaw is
   * NULL, a map containing @a cons as its only entry is returned.
   */
  Slaw MapPut (const Slaw &cons) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new map by adding the key @c k and value @c v to this
   * one (or assigning the latter to the key if it already exists
   * this map). The new map takes ownership of both slawx, so the
   * usual caveats apply. It is legit to provide the same slaw as key
   * and value, though. If any argument is NULL, the receiving Slaw
   * (viewed as a map) is returned. If this Slaw is NULL, a map
   * containing @a k/v as its only entry is returned.
   */
  Slaw MapPut (slaw k, slaw v) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new map by adding the key @c k and value @c v to this
   * one (or assigning the latter to the key if it already exists
   * this map). If any argument is null, this Slaw, viewed as a map,
   * is returned. If this Slaw is NULL, a map containing @a k/v as its
   * only entry is returned.
   */
  Slaw MapPut (const Slaw &k, const Slaw &v) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new map by adding the key @c k and value @c v to this
   * one (or assigning the latter to the key if it already exists
   * this map), creating Slaw values from the corresponding
   * arguments.
   */
  template <typename T, typename U>
  Slaw MapPut (T k, U v) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new map by merging this one with @a other. In case of
   * duplicated keys, @a other's values are used.
   * @note The order of the new map has the elements from this
   * before the elements from @a other.
   */
  Slaw MapMerge (const Slaw &other) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new map which contains the same keys as this one,
   * except for the given one. If @a key is NULL, this Slaw, viewed
   * as a map, is returned. If this is a null Slaw, an empty map is
   * returned.
   */
  Slaw MapRemove (bslaw key) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new map which contains the same keys as this one,
   * except for the given one. This function does @b not take
   * ownership of @a key. If @a key is NULL, this Slaw, viewed as a
   * map, is returned. If this is a null Slaw, an empty map is
   * returned.
   */
  Slaw MapRemove (slaw key) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new map which contains the same keys as this one,
   * except for the given one. If @a key is NULL, this Slaw, viewed
   * as a map, is returned. If this is a null Slaw, an empty map is
   * returned.
   */
  Slaw MapRemove (const Slaw &key) const OB_WARN_UNUSED_RESULT;

  /**
   * Creates a new map which contains the same keys as this one,
   * except for the given one. If this is a null Slaw, an empty map
   * is returned.
   */
  template <typename T>
  Slaw MapRemove (T key) const OB_WARN_UNUSED_RESULT;

  /**
   * Returns the value associated with @a key in this map, or a NULL
   * Slaw (as constructed with Slaw()) if no such value exists. This
   * Slaw is reinterpreted as a map as needed.
   */
  Slaw Find (bslaw key) const;

  /**
   * Returns the value associated with @a key in this map, or a NULL
   * Slaw if no such value exists. This function does @b not take
   * ownership of @a key. If needed, this Slaw is reinterpreted as a
   * map.
   */
  Slaw Find (slaw key) const;

  /**
   * Returns the value associated with @a key in this Slaw, viewed as a
   * map, or a NULL Slaw if no such value exists.
   */
  Slaw Find (const Slaw &key) const;

  /**
   * Template version of the lookup function Find, so that you can
   * provide the key without constructing a Slaw yourself.
   */
  template <typename T>
  Slaw Find (T key) const;

  /**
   * Returns the value associated with @a key in this map, or a NULL
   * Slaw (as constructed with Slaw()) if no such value exists. No
   * reinterpretation of this Slaw as a map is performed: only maps and
   * lists can yield a non-null result. In other words, this function
   * has the same semantics as slaw_map_find applied to SlawValue()
   * with the given key.
   */
  Slaw MapFind (bslaw key) const;

  /**
   * Returns the value associated with @a key in this map, or a NULL
   * Slaw if no such value exists. This function does @b not take
   * ownership of @a key. No reinterpretation of this Slaw as a map is
   * performed: this function works as slaw_map_find in the C API.
   */
  Slaw MapFind (slaw key) const;

  /**
   * Returns the value associated with @a key in this map, or a NULL
   * Slaw if no such value exists. No reinterpretation of this Slaw as
   * a map is performed, unless it is a list or already a map.
   */
  Slaw MapFind (const Slaw &key) const;

  /**
   * Template version of the lookup function MapFind, so that you can
   * provide the key without constructing a Slaw yourself. No
   * reinterpretation of this Slaw as a map is performed: any search on
   * a non-list, non-map will return a null result (this is the
   * slaw_map_find semantics in libPlasma).
   */
  template <typename T>
  Slaw MapFind (T key) const;
  //@}

  /**
   * @name Protein interface
   * A protein is just the composition of a list (descrips) and a map
   * (ingests). Therefore, our interface for breaking down proteins
   * into its constituents is very simple: it just provides access to
   * these parts as Slaw instances, on which you can exercise the
   * whole Slaw interface.
   */
  //@{
  /**
   * Creates an empty Protein, which is however not null.
   */
  static Slaw Protein ();

  /**
   * Creates a protein out of its parts. Thus, @a descrips is
   * interpreted as a list and @a ingests as a map, according to the
   * corresponding coercion rules.
   */
  static Slaw Protein (const Slaw &descrips, const Slaw &ingests);

  /**
   * Returns a list, with this Slaw interpreted as a protein. In case
   * the Slaw is a native protein, this function just returns the
   * obvious value. If this Slaw is a map, an empty list is returned
   * (the map corresponds to the descrips). In all other cases, this
   * Slaw is re-interpreted as a list, which becomes the returned
   * value.
   */
  Slaw Descrips () const;

  /**
   * Returns a map, with this Slaw interpreted as a protein. In case
   * the Slaw is a native protein, this function just returns the
   * obvious value. If this Slaw is a map or a list of conses, this
   * functions returns a copy of this same Slaw. Otherwise, an empty
   * Slaw is returned.
   */
  Slaw Ingests () const;
  //@}

  /**
   * @name Iterators
   * Types and member for STL-compliant iteration through sub-slaws.
   * These types and functions allow us to view the slaw as an STL
   * container, and apply to it algorithms that don't mutate their
   * input.
   */
  //@{
  /**
   * Iterator pointing to the beginning of this Slaw, reinterpreted,
   * if needed, as a list.
   */
  SlawIterator begin () const;

  /**
   * Your typical end marker iterator.
   */
  SlawIterator end () const;
  //@}

 private:
  explicit Slaw (detail::SlawRef slaw);
  explicit Slaw (detail::CompositeSlaw::Ref slaw);

  static Slaw MakeList (slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw,
                        slaw);

  static Slaw MakeList (const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                        const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                        const Slaw &, const Slaw &);

  static Slaw MakeMap (slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw,
                       slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw, slaw,
                       slaw, slaw);

  static Slaw MakeMap (const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                       const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                       const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                       const Slaw &, const Slaw &, const Slaw &, const Slaw &,
                       const Slaw &, const Slaw &, const Slaw &, const Slaw &);

  const detail::CompositeSlaw::Ref &Composite () const;
  const detail::SlawRef &SlawRef () const;
  static detail::SlawRef GetRef (const Slaw &, const detail::SlawRefs &);
  static detail::SlawRef GetRef (slaw s, const detail::SlawRefs &);

  Slaw MapPut (detail::SlawRef key, detail::SlawRef value) const;
  Slaw MapKVs (bool values) const;

  mutable detail::SlawRef slaw_;
  mutable detail::CompositeSlaw::Ref composite_;
};

/* ---------------------------------------------------------------------- */
// Template implementations

template <typename T>
Slaw::Slaw (const T &value) : slaw_ (make_slaw (value))
{
}

template <typename T>
Slaw::Slaw (const T *values, size_t count)
    : slaw_ (slaw_traits<const T *>::Make (values, count))
{
}

template <typename T>
bool Slaw::Into (T &value) const
{
  if (slaw_.IsNull () && composite_->CanEmit<T> ())
    return slaw_traits<T>::Coerce (SlawValue (), value);
  return slaw_traits<T>::Coerce (slaw_, value);
}

template <typename T>
T Slaw::Emit () const
{
  T value (slaw_traits<T>::Null ());
  void(Into (value));
  return value;
}

template <typename T>
bool Slaw::CanEmit () const
{
  if (slaw_.IsNull ())
    return composite_->CanEmit<T> ();
  T dummy;
  return slaw_traits<T>::Coerce (slaw_, dummy);
}

template <typename T>
bool Slaw::Is () const
{
  return slaw_traits<T>::TypeP (slaw_);
}

template <typename T, typename U>
Slaw Slaw::Cons (T car, U cdr)
{
  return Slaw::Cons (Slaw (car), Slaw (cdr));
}

template <typename It>
Slaw Slaw::ListCollect (It b, It e, size_t hint)
{
  detail::SlawRefs components;
  components.reserve (hint);
  for (It i = b; i != e; ++i)
    {
      detail::SlawRef ref (GetRef (*i, components));
      if (!ref.IsNull ())
        components.push_back (ref);
    }
  return Slaw (detail::CompositeSlaw::List (components));
}

template <class T0>
Slaw Slaw::List (T0 s0)
{
  return List (Slaw (s0));
}

template <class T0, class T1>
Slaw Slaw::List (T0 s0, T1 s1)
{
  return List (Slaw (s0), Slaw (s1));
}

template <class T0, class T1, class T2>
Slaw Slaw::List (T0 s0, T1 s1, T2 s2)
{
  return List (Slaw (s0), Slaw (s1), Slaw (s2));
}

template <class T0, class T1, class T2, class T3>
Slaw Slaw::List (T0 s0, T1 s1, T2 s2, T3 s3)
{
  return List (Slaw (s0), Slaw (s1), Slaw (s2), Slaw (s3));
}

template <class T0, class T1, class T2, class T3, class T4>
Slaw Slaw::List (T0 s0, T1 s1, T2 s2, T3 s3, T4 s4)
{
  return List (Slaw (s0), Slaw (s1), Slaw (s2), Slaw (s3), Slaw (s4));
}

template <class T0, class T1, class T2, class T3, class T4, class T5>
Slaw Slaw::List (T0 s0, T1 s1, T2 s2, T3 s3, T4 s4, T5 s5)
{
  return List (Slaw (s0), Slaw (s1), Slaw (s2), Slaw (s3), Slaw (s4),
               Slaw (s5));
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6>
Slaw Slaw::List (T0 s0, T1 s1, T2 s2, T3 s3, T4 s4, T5 s5, T6 s6)
{
  return List (Slaw (s0), Slaw (s1), Slaw (s2), Slaw (s3), Slaw (s4), Slaw (s5),
               Slaw (s6));
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7>
Slaw Slaw::List (T0 s0, T1 s1, T2 s2, T3 s3, T4 s4, T5 s5, T6 s6, T7 s7)
{
  return List (Slaw (s0), Slaw (s1), Slaw (s2), Slaw (s3), Slaw (s4), Slaw (s5),
               Slaw (s6), Slaw (s7));
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8>
Slaw Slaw::List (T0 s0, T1 s1, T2 s2, T3 s3, T4 s4, T5 s5, T6 s6, T7 s7, T8 s8)
{
  return List (Slaw (s0), Slaw (s1), Slaw (s2), Slaw (s3), Slaw (s4), Slaw (s5),
               Slaw (s6), Slaw (s7), Slaw (s8));
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9>
Slaw Slaw::List (T0 s0, T1 s1, T2 s2, T3 s3, T4 s4, T5 s5, T6 s6, T7 s7, T8 s8,
                 T9 s9)
{
  return List (Slaw (s0), Slaw (s1), Slaw (s2), Slaw (s3), Slaw (s4), Slaw (s5),
               Slaw (s6), Slaw (s7), Slaw (s8), Slaw (s9));
}

template <typename T>
int64 Slaw::IndexOf (T value) const
{
  return IndexOf (Slaw (value));
}

template <typename T>
int64 Slaw::IndexOf (T value, unt64 start) const
{
  return IndexOf (Slaw (value), start);
}

template <typename T>
bool Slaw::ListContains (T value) const
{
  return (IndexOf (Slaw (value)) > -1);
}

template <typename T>
bool Slaw::ListContains (T value, unt64 start) const
{
  return (IndexOf (Slaw (value), start) > -1);
}

template <typename T>
Slaw Slaw::ListInsert (T value, int64 n) const
{
  return ListInsert (Slaw (value), n);
}

template <typename T>
Slaw Slaw::ListAppend (T value) const
{
  return ListAppend (Slaw (value));
}

template <typename T>
Slaw Slaw::ListPrepend (T value) const
{
  return ListPrepend (Slaw (value));
}

template <typename It>
Slaw Slaw::MapCollect (It i, It e, size_t hint)
{
  detail::SlawRefs components;
  components.reserve (hint);
  for (; i != e; ++i)
    components.push_back (GetRef (*i, components));
  return Slaw (detail::CompositeSlaw::Map (components));
}

template <class T0, class T1>
Slaw Slaw::Map (T0 t0, T1 t1)
{
  return Map (Slaw (t0), Slaw (t1));
}

template <class T0, class T1, class T2, class T3>
Slaw Slaw::Map (T0 t0, T1 t1, T2 t2, T3 t3)
{
  return Map (Slaw (t0), Slaw (t1), Slaw (t2), Slaw (t3));
}

template <class T0, class T1, class T2, class T3, class T4, class T5>
Slaw Slaw::Map (T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5)
{
  return Map (Slaw (t0), Slaw (t1), Slaw (t2), Slaw (t3), Slaw (t4), Slaw (t5));
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7>
Slaw Slaw::Map (T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7)
{
  return Map (Slaw (t0), Slaw (t1), Slaw (t2), Slaw (t3), Slaw (t4), Slaw (t5),
              Slaw (t6), Slaw (t7));
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9>
Slaw Slaw::Map (T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8,
                T9 t9)
{
  return Map (Slaw (t0), Slaw (t1), Slaw (t2), Slaw (t3), Slaw (t4), Slaw (t5),
              Slaw (t6), Slaw (t7), Slaw (t8), Slaw (t9));
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11>
Slaw Slaw::Map (T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8,
                T9 t9, T10 t10, T11 t11)
{
  return Map (Slaw (t0), Slaw (t1), Slaw (t2), Slaw (t3), Slaw (t4), Slaw (t5),
              Slaw (t6), Slaw (t7), Slaw (t8), Slaw (t9), Slaw (t10),
              Slaw (t11));
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13>
Slaw Slaw::Map (T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8,
                T9 t9, T10 t10, T11 t11, T12 t12, T13 t13)
{
  return Map (Slaw (t0), Slaw (t1), Slaw (t2), Slaw (t3), Slaw (t4), Slaw (t5),
              Slaw (t6), Slaw (t7), Slaw (t8), Slaw (t9), Slaw (t10),
              Slaw (t11), Slaw (t12), Slaw (t13));
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13, class T14, class T15>
Slaw Slaw::Map (T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8,
                T9 t9, T10 t10, T11 t11, T12 t12, T13 t13, T14 t14, T15 t15)
{
  return Map (Slaw (t0), Slaw (t1), Slaw (t2), Slaw (t3), Slaw (t4), Slaw (t5),
              Slaw (t6), Slaw (t7), Slaw (t8), Slaw (t9), Slaw (t10),
              Slaw (t11), Slaw (t12), Slaw (t13), Slaw (t14), Slaw (t15));
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13, class T14, class T15, class T16, class T17>
Slaw Slaw::Map (T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8,
                T9 t9, T10 t10, T11 t11, T12 t12, T13 t13, T14 t14, T15 t15,
                T16 t16, T17 t17)
{
  return Map (Slaw (t0), Slaw (t1), Slaw (t2), Slaw (t3), Slaw (t4), Slaw (t5),
              Slaw (t6), Slaw (t7), Slaw (t8), Slaw (t9), Slaw (t10),
              Slaw (t11), Slaw (t12), Slaw (t13), Slaw (t14), Slaw (t15),
              Slaw (t16), Slaw (t17));
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6,
          class T7, class T8, class T9, class T10, class T11, class T12,
          class T13, class T14, class T15, class T16, class T17, class T18,
          class T19>
Slaw Slaw::Map (T0 t0, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8,
                T9 t9, T10 t10, T11 t11, T12 t12, T13 t13, T14 t14, T15 t15,
                T16 t16, T17 t17, T18 t18, T19 t19)
{
  return Map (Slaw (t0), Slaw (t1), Slaw (t2), Slaw (t3), Slaw (t4), Slaw (t5),
              Slaw (t6), Slaw (t7), Slaw (t8), Slaw (t9), Slaw (t10),
              Slaw (t11), Slaw (t12), Slaw (t13), Slaw (t14), Slaw (t15),
              Slaw (t16), Slaw (t17), Slaw (t18), Slaw (t19));
}

template <typename T, typename U>
Slaw Slaw::MapPut (T key, U value) const
{
  return MapPut (Slaw (key), Slaw (value));
}

template <typename T>
Slaw Slaw::MapRemove (T key) const
{
  return MapRemove (Slaw (key));
}

template <typename T>
Slaw Slaw::Find (T key) const
{
  return Find (Slaw (key));
}

template <typename T>
Slaw Slaw::MapFind (T key) const
{
  return MapFind (Slaw (key));
}

template <typename T>
int64 Slaw::ListFind (T key) const
{
  return ListFind (Slaw (key));
}

// http://www.boost.org/doc/libs/1_47_0/doc/html/hash/custom.html
inline size_t hash_value (const Slaw &s)
{
  return static_cast<size_t> (s.Hash ());
}

/**
 * A hash function wrapper that makes Slaw easy to use in Boost or TR1
 * unordered maps.
 */
struct Slaw_hash
{
  size_t operator() (const Slaw &s) const
  {
    return static_cast<size_t> (s.Hash ());
  };
};

/**
 * Trivial self-emission/absorption, for use with Emit, slaw_cast<> etc.
 */
#define PLATFORM_SLAWXX_SELF_EMISSION
template <>
struct slaw_traits<Slaw>
{
  typedef Slaw type;
  static type Null () { return Slaw::Null (); }
  static bool TypeP (bslaw) { return true; }
  static bool Coerce (bslaw s, type &value)
  {
    value = Slaw (slaw_dup (s));
    return true;
  }
  static slaw Make (type v) { return slaw_dup (v.SlawValue ()); }
};
}
}  // namespace oblong::plasma


namespace std {

template <>
struct hash<oblong::plasma::Slaw>
{
  typedef oblong::plasma::Slaw argument_type;
  typedef std::size_t result_type;

  result_type operator() (const argument_type &s) const
  {
    return static_cast<size_t> (s.Hash ());
  }
};
}


#endif  // OBLONG_PLASMA_SLAW_H
