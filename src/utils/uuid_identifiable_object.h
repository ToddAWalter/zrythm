// SPDX-FileCopyrightText: © 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "utils/format.h"
#include "utils/icloneable.h"
#include "utils/initializable_object.h"
#include "utils/iserializable.h"

#include <QUuid>

#include "type_safe/strong_typedef.hpp"
#include <boost/stl_interfaces/iterator_interface.hpp>

namespace zrythm::utils
{
/**
 * Base class for objects that need to be uniquely identified by UUID.
 */
template <typename Derived>
class UuidIdentifiableObject
    : public serialization::ISerializable<UuidIdentifiableObject<Derived>>
{
public:
  struct Uuid
      : type_safe::strong_typedef<Uuid, QUuid>,
        type_safe::strong_typedef_op::equality_comparison<Uuid>,
        type_safe::strong_typedef_op::relational_comparison<Uuid>,
        utils::serialization::ISerializable<Uuid>
  {
    using UuidTag = void; // used by the fmt formatter below
    using type_safe::strong_typedef<Uuid, QUuid>::strong_typedef;

    explicit Uuid () = default;

    static_assert (StrongTypedef<Uuid>);
    // static_assert (type_safe::is_strong_typedef<Uuid>::value);
    // static_assert (std::is_same_v<type_safe::underlying_type<Uuid>, QUuid>);

    /**
     * @brief Checks if the UUID is null.
     */
    bool is_null () const { return type_safe::get (*this).isNull (); }

    void set_null () { type_safe::get (*this) = QUuid (); }

    DECLARE_DEFINE_FIELDS_METHOD ()
    {
      using T = serialization::ISerializable<Uuid>;
      T::serialize_fields (ctx, T::make_field ("uuid", type_safe::get (*this)));
    }

    std::size_t hash () const { return qHash (type_safe::get (*this)); }
  };
  static_assert (std::regular<Uuid>);

  UuidIdentifiableObject () : uuid_ (Uuid (QUuid::createUuid ())) { }
  UuidIdentifiableObject (const Uuid &id) : uuid_ (id) { }
  UuidIdentifiableObject (const UuidIdentifiableObject &other) = default;
  UuidIdentifiableObject &
  operator= (const UuidIdentifiableObject &other) = default;
  UuidIdentifiableObject (UuidIdentifiableObject &&other) = default;
  UuidIdentifiableObject &operator= (UuidIdentifiableObject &&other) = default;
  ~UuidIdentifiableObject () override = default;

  auto get_uuid () const { return uuid_; }

  void copy_members_from (const UuidIdentifiableObject &other)
  {
    uuid_ = other.uuid_;
  }

  DECLARE_DEFINE_BASE_FIELDS_METHOD ()
  {
    using T = UuidIdentifiableObject;
    T::serialize_fields (ctx, T::make_field ("id", uuid_));
  }

private:
  Uuid uuid_;
};

template <typename T>
concept UuidIdentifiable = std::derived_from<T, UuidIdentifiableObject<T>>;

template <typename T>
concept UuidIdentifiableQObject =
  UuidIdentifiable<T> && std::derived_from<T, QObject>;

// specialization for std::hash and QHash
#define DEFINE_UUID_HASH_SPECIALIZATION(UuidType) \
  namespace std \
  { \
  template <> struct hash<UuidType> \
  { \
    size_t operator() (const UuidType &uuid) const \
    { \
      return uuid.hash (); \
    } \
  }; \
  }

/**
 * @brief A reference-counted RAII wrapper for a UUID in a registry.
 *
 * Objects that refer to an object's UUID must use this wrapper.
 *
 * TODO: actually use this
 *
 * @tparam RegistryT
 */
template <typename RegistryT>
class UuidReference
    : public serialization::ISerializable<UuidReference<RegistryT>>
{
public:
  using UuidType = typename RegistryT::UuidType;
  using VariantType = typename RegistryT::VariantType;

  // FIXME: temporary workaround so that Tracklist::swap_tracks() works with
  // temporary resizing of tracks_. This should be removed after the swap logic
  // is refactored.
  UuidReference () = default;

  UuidReference (const DeserializationDependencyHolder &dh)
      : registry_ (dh.get<std::reference_wrapper<RegistryT>> ().get ())
  {
  }

  UuidReference (const UuidType &id, RegistryT &registry)
      : id_ (id), registry_ (registry)
  {
    acquire_ref ();
  }

  UuidReference (const UuidReference &other)
      : UuidReference (other.id (), other.get_registry ())
  {
  }
  UuidReference &operator= (const UuidReference &other)
  {
    if (this != &other)
      {
        id_ = other.id_;
        acquire_ref ();
      }
    return *this;
  }

  UuidReference (UuidReference &&other)
  {
    if (this != &other)
      {
        id_ = std::move (other.id_);
        registry_ = std::move (other.registry_);
        acquire_ref ();
      }
  }
  UuidReference &operator= (UuidReference &&other)
  {
    if (this != &other)
      {
        id_ = std::move (other.id_);
        registry_ = std::move (other.registry_);
        acquire_ref ();
      }
    return *this;
  }

  ~UuidReference () { release_ref (); }

  const UuidType &id () const { return *id_; }

  VariantType get_object () const
  {
    return get_registry ().find_by_id_or_throw (id ());
  }

  template <typename... Args>
  UuidReference clone_new_identity (Args &&... args) const
  {
    return std::visit (
      [&] (auto &&obj) {
        return get_registry ().clone_object (*obj, std::forward<Args> (args)...);
      },
      get_object ());
  }

  DECLARE_DEFINE_FIELDS_METHOD ()
  {
    using T = serialization::ISerializable<UuidReference>;
    T::serialize_fields (ctx, T::make_field ("uuid", id_));

    if (ctx.is_deserializing ())
      {
        acquire_ref ();
      }
  }

  auto get_iterator_in_registry () const
  {
    return get_registry ().get_iterator_for_id (id ());
  }

  auto get_iterator_in_registry ()
  {
    return get_registry ().get_iterator_for_id (id ());
  }

  friend bool operator== (const UuidReference &lhs, const UuidReference &rhs)
  {
    return lhs.id () == rhs.id ();
  }

private:
  void acquire_ref ()
  {
    if (id_.has_value ())
      {
        get_registry ().acquire_reference (*id_);
      }
  }
  void release_ref ()
  {
    if (id_.has_value ())
      {
        get_registry ().release_reference (*id_);
      }
  }
  RegistryT &get_registry () const
  {
    return const_cast<RegistryT &> (*registry_);
  }
  RegistryT &get_registry () { return *registry_; }

private:
  std::optional<UuidType> id_;
  OptionalRef<RegistryT>  registry_;
};

template <typename ReturnType, typename UuidType>
using UuidIdentifiablObjectResolver =
  std::function<ReturnType (const UuidType &)>;

/**
 * @brief A registry that owns and manages objects identified by a UUID.
 *
 * This class provides methods to register, unregister, and find objects by
 * their UUID. When an object is registered, this class takes ownership of it
 * and sets the parent to `this`. When an object is unregistered, the ownership
 * is released and the caller is responsible for deleting the object.
 *
 * Intended to be used with variants of pointers to QObjects.
 *
 * @note Object registration and deregistration must only be done from the main
 * thread.
 *
 * @tparam VariantT A variant of raw pointers to QObject-derived classes.
 * @tparam BaseT The common base class of the objects in the variant.
 */
template <typename VariantT, UuidIdentifiable BaseT>
class OwningObjectRegistry
    : public QObject,
      public serialization::ISerializable<OwningObjectRegistry<VariantT, BaseT>>
{
public:
  using UuidType = typename UuidIdentifiableObject<BaseT>::Uuid;
  using VariantType = VariantT;
  using BaseType = BaseT;

  OwningObjectRegistry (QObject * parent = nullptr) : QObject (parent) { }

  // Factory method that forwards constructor arguments
  template <typename CreateType, typename... Args>
  auto create_object (Args &&... args) -> UuidReference<OwningObjectRegistry>
    requires std::derived_from<CreateType, BaseT>
  {
    CreateType * obj{};
    if constexpr (std::derived_from<CreateType, InitializableObject>)
      {
        auto obj_unique_ptr = CreateType::template create_unique<CreateType> (
          std::forward<Args> (args)...);
        obj = obj_unique_ptr.release ();
      }
    else
      {
        obj = new CreateType (std::forward<Args> (args)...);
      }
    z_trace (
      "created object of type {} with ID {}", typeid (CreateType).name (),
      obj->get_uuid ());
    register_object (obj);
    return { obj->get_uuid (), *this };
  }

  // Creates a clone of the given object and registers it to the registry.
  template <typename CreateType, typename... Args>
  auto clone_object (const CreateType &other, Args &&... args)
    -> UuidReference<OwningObjectRegistry>
    requires std::derived_from<CreateType, BaseT>
  {
    CreateType * obj = other.clone_raw_ptr (
      ObjectCloneType::NewIdentity, std::forward<Args> (args)...);
    register_object (obj);
    return { obj->get_uuid (), *this };
  }

  [[gnu::hot]] auto get_iterator_for_id (const UuidType &id) const
  {
    const auto &uuid = type_safe::get (id);
    return objects_by_id_.constFind (uuid);
  }

  [[gnu::hot]] auto get_iterator_for_id (const UuidType &id)
  {
    const auto &uuid = type_safe::get (id);
    return objects_by_id_.find (uuid);
  }

  /**
   * @brief Returns an object by id.
   *
   * @return A non-owning pointer to the object.
   */
  [[gnu::hot]] std::optional<std::reference_wrapper<const VariantT>>
  find_by_id (const UuidType &id) const
  {
    const auto it = get_iterator_for_id (id);
    if (it == objects_by_id_.end ())
      {
        return std::nullopt;
      }
    return *it;
  }

  [[gnu::hot]] auto &find_by_id_or_throw (const UuidType &id) const
  {
    auto val = find_by_id (id);
    if (!val.has_value ())
      {
        throw std::runtime_error (
          fmt::format ("Object with id {} not found", id));
      }
    return val.value ().get ();
  }

  bool contains (const UuidType &id) const
  {
    const auto &uuid = type_safe::get (id);
    return objects_by_id_.contains (uuid);
  }

  /**
   * @brief Registers an object.
   *
   * This takes ownership of the object.
   */
  void register_object (VariantT obj_ptr)
  {
    std::visit (
      [&] (auto &&obj) {
        if (contains (obj->get_uuid ()))
          {
            throw std::runtime_error (fmt::format (
              "Object with id {} already exists", obj->get_uuid ()));
          }
        z_trace ("Registering (inserting) object {}", obj->get_uuid ());
        obj->setParent (this);
        objects_by_id_.insert (type_safe::get (obj->get_uuid ()), obj);
      },
      obj_ptr);
  }

  template <typename ObjectT>
  void register_object (ObjectT &obj)
    requires std::derived_from<ObjectT, BaseT>
  {
    register_object (&obj);
  }

  void acquire_reference (const UuidType &id)
  {
    const auto &quuid = type_safe::get (id);
    ref_counts_[quuid]++;
  }

  void release_reference (const UuidType &id)
  {
    const auto &quuid = type_safe::get (id);
    if (--ref_counts_[quuid] <= 0)
      {
        delete_object_by_id (id);
      }
  }

  auto &get_hash_map () const { return objects_by_id_; }

  /**
   * @brief Returns a list of all UUIDs of the objects in the registry.
   */
  auto get_uuids () const
  {
    return objects_by_id_.keys () | std::views::transform ([] (const auto &uuid) {
             return UuidType (uuid);
           })
           | std::ranges::to<std::vector> ();
  }

  size_t size () const { return objects_by_id_.size (); }

  DECLARE_DEFINE_FIELDS_METHOD ()
  {
    using T = serialization::ISerializable<OwningObjectRegistry>;
    T::serialize_fields (ctx, T::make_field ("objectsById", objects_by_id_));

    if (ctx.is_deserializing ())
      {
        for (auto &var : objects_by_id_.values ())
          {
            std::visit ([&] (auto &&v) { v->setParent (this); }, var);
          }
      }
  }

private:
  /**
   * @brief Unregisters an object.
   *
   * This releases ownership of the object and the caller is responsible for
   * deleting it.
   */
  VariantT unregister_object (const UuidType &id)
  {
    if (!objects_by_id_.contains (type_safe::get (id)))
      {
        throw std::runtime_error (
          fmt::format ("Object with id {} not found", id));
      }

    z_trace ("Unregistering object with id {}", id);

    auto obj_var = objects_by_id_.take (type_safe::get (id));
    std::visit ([&] (auto &&obj) { obj->setParent (nullptr); }, obj_var);
    if (ref_counts_.contains (type_safe::get (id)))
      {
        ref_counts_.remove (type_safe::get (id));
      }
    return obj_var;
  }

  void delete_object_by_id (const UuidType &id)
  {
    auto obj_var = unregister_object (id);
    std::visit ([&] (auto &&obj) { delete obj; }, obj_var);
  }

private:
  QHash<QUuid, VariantT> objects_by_id_;
  QHash<QUuid, int>      ref_counts_;
};

template <typename RegistryT> class UuidIdentifiableObjectSelectionManager
{
  using UuidType = typename RegistryT::UuidType;

public:
  using UuidSet = std::unordered_set<UuidType>;

  UuidIdentifiableObjectSelectionManager (
    UuidSet         &selected_objs,
    const RegistryT &registry)
      : selected_objects_ (selected_objs), registry_ (registry)
  {
  }
  void append_to_selection (const UuidType &id)
  {
    if (!is_selected (id))
      {
        selected_objects_.insert (id);
        emit_selection_changed_for_object (id);
      }
  }
  void remove_from_selection (const UuidType &id)
  {
    if (is_selected (id))
      {
        selected_objects_.erase (id);
        emit_selection_changed_for_object (id);
      }
  }
  void select_unique (const UuidType &id)
  {
    clear_selection ();
    append_to_selection (id);
  }
  bool is_selected (const UuidType &id) const
  {
    return selected_objects_.contains (id);
  }
  bool is_only_selection (const UuidType &id) const
  {
    return selected_objects_.size () == 1 && is_selected (id);
  }
  bool empty () const { return selected_objects_.empty (); }
  auto size () const { return selected_objects_.size (); }

  void clear_selection ()
  { // Make a copy of the selected tracks to iterate over
    auto selected_objs_copy = selected_objects_;
    for (const auto &uuid : selected_objs_copy)
      {
        selected_objects_.erase (uuid);
        emit_selection_changed_for_object (uuid);
      }
  }

  template <RangeOf<UuidType> UuidRange>
  void select_only_these (const UuidRange &uuids)
  {
    clear_selection ();
    for (const auto &uuid : uuids)
      {
        append_to_selection (uuid);
      }
  }

private:
  auto get_object_for_id (const UuidType &id)
  {
    return registry_.find_by_id_or_throw (id);
  }
  void emit_selection_changed_for_object (const UuidType &id)
  {
    std::visit (
      [&] (auto &&obj) { Q_EMIT obj->selectedChanged (is_selected (id)); },
      get_object_for_id (id));
  }

private:
  UuidSet         &selected_objects_;
  const RegistryT &registry_;
};

/**
 * @brief A unified view over UUID-identified objects that supports:
 * - Range of VariantType (direct object pointers)
 * - Range of UuidReference
 * - Range of Uuid + Registry
 */
template <typename RegistryT>
class UuidIdentifiableObjectView
    : public std::ranges::view_interface<UuidIdentifiableObjectView<RegistryT>>
{
public:
  using UuidType = typename RegistryT::UuidType;
  using VariantType = typename RegistryT::VariantType;
  using UuidRefType = UuidReference<RegistryT>;

  // Proxy iterator implementation using Boost.STLInterfaces
  class Iterator
      : public boost::stl_interfaces::
          proxy_iterator_interface<std::random_access_iterator_tag, VariantType>
  {
  public:
    using base_type = boost::stl_interfaces::
      proxy_iterator_interface<std::random_access_iterator_tag, VariantType>;
    using difference_type = base_type::difference_type;

    constexpr Iterator () noexcept = default;

    // Constructor for direct object range
    constexpr Iterator (std::span<const VariantType>::iterator it) noexcept
        : it_var_ (it)
    {
    }

    // Constructor for UuidReference range
    constexpr Iterator (std::span<const UuidRefType>::iterator it) noexcept
        : it_var_ (it)
    {
    }

    // Constructor for Uuid + Registry range
    constexpr Iterator (
      std::span<const UuidType>::iterator it,
      const RegistryT *                   registry) noexcept
        : it_var_ (std::pair{ it, registry })
    {
    }

    constexpr VariantType operator* () const
    {
      return std::visit (
        [] (auto &&arg) {
          using T = std::decay_t<decltype (arg)>;
          if constexpr (
            std::is_same_v<T, typename std::span<const VariantType>::iterator>)
            {
              return *arg;
            }
          else if constexpr (
            std::is_same_v<T, typename std::span<const UuidRefType>::iterator>)
            {
              return arg->get_object ();
            }
          else if constexpr (
            std::is_same_v<
              T,
              std::pair<
                typename std::span<const UuidType>::iterator, const RegistryT *>>)
            {
              return arg.second->find_by_id_or_throw (*arg.first);
            }
        },
        it_var_);
    }

    constexpr Iterator &operator+= (difference_type n) noexcept
    {
      std::visit (
        [n] (auto &&arg) {
          using T = std::decay_t<decltype (arg)>;
          if constexpr (
            std::is_same_v<T, typename std::span<const VariantType>::iterator>)
            {
              arg += n;
            }
          else if constexpr (
            std::is_same_v<T, typename std::span<const UuidRefType>::iterator>)
            {
              arg += n;
            }
          else if constexpr (
            std::is_same_v<
              T,
              std::pair<
                typename std::span<const UuidType>::iterator, const RegistryT *>>)
            {
              arg.first += n;
            }
        },
        it_var_);
      return *this;
    }

    constexpr difference_type operator- (Iterator other) const
    {
      return std::visit (
        [] (auto &&arg, auto &&other_arg) -> difference_type {
          using T = std::decay_t<decltype (arg)>;
          using OtherT = std::decay_t<decltype (other_arg)>;
          if constexpr (!std::is_same_v<T, OtherT>)
            {
              throw std::runtime_error (
                "Comparing iterators of different types");
            }
          else if constexpr (
            std::is_same_v<T, typename std::span<const VariantType>::iterator>)
            {
              return arg - other_arg;
            }
          else if constexpr (
            std::is_same_v<T, typename std::span<const UuidRefType>::iterator>)
            {
              return arg - other_arg;
            }
          else if constexpr (
            std::is_same_v<
              T,
              std::pair<
                typename std::span<const UuidType>::iterator, const RegistryT *>>)
            {
              return arg.first - other_arg.first;
            }
          else
            {
              return 0;
            }
        },
        it_var_, other.it_var_);
    }

    constexpr auto operator<=> (const Iterator &other) const
    {
      return std::visit (
        [] (auto &&arg, auto &&other_arg) -> std::strong_ordering {
          using T = std::decay_t<decltype (arg)>;
          using OtherT = std::decay_t<decltype (other_arg)>;
          if constexpr (!std::is_same_v<T, OtherT>)
            {
              throw std::runtime_error (
                "Comparing iterators of different types");
            }
          else if constexpr (
            std::is_same_v<T, typename std::span<const VariantType>::iterator>)
            {
              return arg <=> other_arg;
            }
          else if constexpr (
            std::is_same_v<T, typename std::span<const UuidRefType>::iterator>)
            {
              return arg <=> other_arg;
            }
          else if constexpr (
            std::is_same_v<
              T,
              std::pair<
                typename std::span<const UuidType>::iterator, const RegistryT *>>)
            {
              return arg.first <=> other_arg.first;
            }
          else
            {
              return std::strong_ordering::equal;
            }
        },
        it_var_, other.it_var_);
    }

  private:
    std::variant<
      typename std::span<const VariantType>::iterator,
      typename std::span<const UuidRefType>::iterator,
      std::pair<typename std::span<const UuidType>::iterator, const RegistryT *>>
      it_var_;
  };

  /// Constructor for direct object range
  explicit UuidIdentifiableObjectView (std::span<const VariantType> objects)
      : objects_ (objects)
  {
  }

  /// Constructor for UuidReference range
  explicit UuidIdentifiableObjectView (std::span<const UuidRefType> refs)
      : refs_ (refs)
  {
  }

  /// Constructor for Uuid + Registry
  explicit UuidIdentifiableObjectView (
    const RegistryT          &registry,
    std::span<const UuidType> uuids)
      : uuids_ (uuids), registry_ (&registry)
  {
  }

  /// Single object constructor
  explicit UuidIdentifiableObjectView (const VariantType &obj)
      : objects_ (std::span<const VariantType> (&obj, 1))
  {
  }

  Iterator begin () const
  {
    if (objects_)
      {
        return Iterator (objects_->begin ());
      }
    else if (refs_)
      {
        return Iterator (refs_->begin ());
      }
    else
      {
        return Iterator (uuids_->begin (), registry_);
      }
  }

  Iterator end () const
  {
    if (objects_)
      {
        return Iterator (objects_->end ());
      }
    else if (refs_)
      {
        return Iterator (refs_->end ());
      }
    else
      {
        return Iterator (uuids_->end (), registry_);
      }
  }

  VariantType operator[] (size_t index) const
  {
    if (objects_)
      return (*objects_)[index];
    if (refs_)
      return (*refs_)[index].get_object ();
    return registry_->find_by_id_or_throw ((*uuids_)[index]);
  }

  VariantType front () const { return *begin (); }
  VariantType back () const
  {
    if (empty ())
      throw std::out_of_range ("Cannot get back() of empty view");

    auto it = end ();
    --it;
    return *it;
  }
  VariantType at (size_t index) const
  {
    if (index >= size ())
      throw std::out_of_range (
        "UuidIdentifiableObjectView::at index out of range");

    return (*this)[index];
  }

  size_t size () const
  {
    if (objects_)
      return objects_->size ();
    if (refs_)
      return refs_->size ();
    return uuids_->size ();
  }

  bool empty () const { return size () == 0; }

  // Projections and transformations
  static UuidType uuid_projection (const VariantType &var)
  {
    return std::visit ([] (const auto &obj) { return obj->get_uuid (); }, var);
  }

  template <typename T> auto get_elements_by_type () const
  {
    return *this | std::views::filter ([] (const auto &var) {
      return std::holds_alternative<T *> (var);
    }) | std::views::transform ([] (const auto &var) {
      return std::get<T *> (var);
    });
  }

  static RegistryT::BaseType * base_projection (const VariantType &var)
  {
    return std::visit (
      [] (const auto &ptr) -> RegistryT::BaseType * { return ptr; }, var);
  }

  auto as_base_type () const
  {
    return std::views::transform (*this, base_projection);
  }

  template <typename T> static auto type_projection (const VariantType &var)
  {
    return std::holds_alternative<T *> (var);
  }

  template <typename T> bool contains_type () const
  {
    return std::ranges::any_of (*this, type_projection<T>);
  }

  template <typename BaseType>
  static auto derived_from_type_projection (const VariantType &var)
  {
    return std::visit (
      [] (const auto &ptr) {
        return std::derived_from<base_type<decltype (ptr)>, BaseType>;
      },
      var);
  }

  /**
   * @note Assumes the range has already been filtered to only contain the type
   * T.
   */
  template <typename T> static auto type_transformation (const VariantType &var)
  {
    return std::get<T *> (var);
  }

  template <typename T>
  static auto derived_from_type_transformation (const VariantType &var)
  {
    return std::visit (
      [] (const auto &ptr) -> T * {
        using ElementT = base_type<decltype (ptr)>;
        if constexpr (std::derived_from<ElementT, T>)
          return ptr;
        throw std::runtime_error ("Not derived from type");
      },
      var);
  }

  // note: assumes all objects are of this type
  template <typename T> auto as_type () const
  {
    return std::views::transform (*this, type_transformation<T>);
  }

  template <typename T> auto get_elements_derived_from () const
  {
    return *this | std::views::filter (derived_from_type_projection<T>)
           | std::views::transform (derived_from_type_transformation<T>);
  }

private:
  std::optional<std::span<const VariantType>> objects_;
  std::optional<std::span<const UuidRefType>> refs_;
  std::optional<std::span<const UuidType>>    uuids_;
  const RegistryT *                           registry_ = nullptr;
};

} // namespace zrythm::utils

DEFINE_OBJECT_FORMATTER (QUuid, utils_uuid, [] (const auto &uuid) {
  return fmt::format ("{}", uuid.toString (QUuid::WithoutBraces));
})

// Concept to detect UuidIdentifiableObject::Uuid types
template <typename T>
concept UuidType = requires (T t) {
  typename T::UuidTag;
  { type_safe::get (t) } -> std::convertible_to<QUuid>;
};

// Formatter for any UUID type from UuidIdentifiableObject
template <UuidType T>
struct fmt::formatter<T> : fmt::formatter<std::string_view>
{
  template <typename FormatContext>
  auto format (const T &uuid, FormatContext &ctx) const
  {
    return fmt::formatter<std::string_view>::format (
      type_safe::get (uuid).toString (QUuid::WithoutBraces).toStdString (), ctx);
  }
};

// Concept to detect UuidReference types
template <typename T>
concept UuidReferenceType = requires (T t) {
  { type_safe::get (t.id ()) } -> std::convertible_to<QUuid>;
};

// Formatter for UuidReference
template <UuidReferenceType T>
struct fmt::formatter<T> : fmt::formatter<std::string_view>
{
  template <typename FormatContext>
  auto format (const T &uuid_ref, FormatContext &ctx) const
  {
    return fmt::formatter<std::string_view>::format (
      fmt::format ("{}", uuid_ref.id ()), ctx);
  }
};
