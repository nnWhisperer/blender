/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_tree_interface_types.h"
#include "DNA_node_types.h"

#include "BKE_node.h"

#include <queue>
#include <type_traits>

#include "BLI_cache_mutex.hh"
#include "BLI_parameter_pack_utils.hh"
#include "BLI_vector.hh"

namespace blender::bke {

class NodeTreeMainUpdater;

class bNodeTreeInterfaceRuntime {
  friend bNodeTreeInterface;
  friend bNodeTree;

 private:
  /**
   * Keeps track of what changed in the node tree until the next update.
   * Should not be changed directly, instead use the functions in `BKE_node_tree_update.h`.
   * #NodeTreeInterfaceChangedFlag.
   */
  uint32_t changed_flag_ = 0;

  /**
   * Protects access to item cache variables below. This is necessary so that the cache can be
   * updated on a const #bNodeTreeInterface.
   */
  CacheMutex items_cache_mutex_;

  /* Runtime topology cache for linear access to items. */
  Vector<bNodeTreeInterfaceItem *> items_;
  /* Socket-only lists for input/output access by index. */
  Vector<bNodeTreeInterfaceSocket *> inputs_;
  Vector<bNodeTreeInterfaceSocket *> outputs_;
};

namespace node_interface {

namespace detail {

template<typename T> static bool item_is_type(const bNodeTreeInterfaceItem &item)
{
  bool match = false;
  switch (item.item_type) {
    case NODE_INTERFACE_SOCKET: {
      match |= std::is_same<T, bNodeTreeInterfaceSocket>::value;
      break;
    }
    case NODE_INTERFACE_PANEL: {
      match |= std::is_same<T, bNodeTreeInterfacePanel>::value;
      break;
    }
  }
  return match;
}

}  // namespace detail

template<typename T> T &get_item_as(bNodeTreeInterfaceItem &item)
{
  BLI_assert(detail::item_is_type<T>(item));
  return reinterpret_cast<T &>(item);
}

template<typename T> const T &get_item_as(const bNodeTreeInterfaceItem &item)
{
  BLI_assert(detail::item_is_type<T>(item));
  return reinterpret_cast<const T &>(item);
}

template<typename T> T *get_item_as(bNodeTreeInterfaceItem *item)
{
  if (item && detail::item_is_type<T>(*item)) {
    return reinterpret_cast<T *>(item);
  }
  return nullptr;
}

template<typename T> const T *get_item_as(const bNodeTreeInterfaceItem *item)
{
  if (item && detail::item_is_type<T>(*item)) {
    return reinterpret_cast<const T *>(item);
  }
  return nullptr;
}

namespace socket_types {

/* Info for generating static subtypes. */
struct bNodeSocketStaticTypeInfo {
  const char *socket_identifier;
  const char *interface_identifier;
  eNodeSocketDatatype type;
  PropertySubType subtype;
  const char *label;
};

/* Note: Socket and interface subtypes could be defined from a single central list,
 * but makesrna cannot have a dependency on BKE, so this list would have to live in RNA itself,
 * with BKE etc. accessing the RNA API to get the subtypes info. */
static const bNodeSocketStaticTypeInfo node_socket_subtypes[] = {
    {"NodeSocketFloat", "NodeTreeInterfaceSocketFloat", SOCK_FLOAT, PROP_NONE},
    {"NodeSocketFloatUnsigned", "NodeTreeInterfaceSocketFloatUnsigned", SOCK_FLOAT, PROP_UNSIGNED},
    {"NodeSocketFloatPercentage",
     "NodeTreeInterfaceSocketFloatPercentage",
     SOCK_FLOAT,
     PROP_PERCENTAGE},
    {"NodeSocketFloatFactor", "NodeTreeInterfaceSocketFloatFactor", SOCK_FLOAT, PROP_FACTOR},
    {"NodeSocketFloatAngle", "NodeTreeInterfaceSocketFloatAngle", SOCK_FLOAT, PROP_ANGLE},
    {"NodeSocketFloatTime", "NodeTreeInterfaceSocketFloatTime", SOCK_FLOAT, PROP_TIME},
    {"NodeSocketFloatTimeAbsolute",
     "NodeTreeInterfaceSocketFloatTimeAbsolute",
     SOCK_FLOAT,
     PROP_TIME_ABSOLUTE},
    {"NodeSocketFloatDistance", "NodeTreeInterfaceSocketFloatDistance", SOCK_FLOAT, PROP_DISTANCE},
    {"NodeSocketInt", "NodeTreeInterfaceSocketInt", SOCK_INT, PROP_NONE},
    {"NodeSocketIntUnsigned", "NodeTreeInterfaceSocketIntUnsigned", SOCK_INT, PROP_UNSIGNED},
    {"NodeSocketIntPercentage", "NodeTreeInterfaceSocketIntPercentage", SOCK_INT, PROP_PERCENTAGE},
    {"NodeSocketIntFactor", "NodeTreeInterfaceSocketIntFactor", SOCK_INT, PROP_FACTOR},
    {"NodeSocketBool", "NodeTreeInterfaceSocketBool", SOCK_BOOLEAN, PROP_NONE},
    {"NodeSocketRotation", "NodeTreeInterfaceSocketRotation", SOCK_ROTATION, PROP_NONE},
    {"NodeSocketVector", "NodeTreeInterfaceSocketVector", SOCK_VECTOR, PROP_NONE},
    {"NodeSocketVectorTranslation",
     "NodeTreeInterfaceSocketVectorTranslation",
     SOCK_VECTOR,
     PROP_TRANSLATION},
    {"NodeSocketVectorDirection",
     "NodeTreeInterfaceSocketVectorDirection",
     SOCK_VECTOR,
     PROP_DIRECTION},
    {"NodeSocketVectorVelocity",
     "NodeTreeInterfaceSocketVectorVelocity",
     SOCK_VECTOR,
     PROP_VELOCITY},
    {"NodeSocketVectorAcceleration",
     "NodeTreeInterfaceSocketVectorAcceleration",
     SOCK_VECTOR,
     PROP_ACCELERATION},
    {"NodeSocketVectorEuler", "NodeTreeInterfaceSocketVectorEuler", SOCK_VECTOR, PROP_EULER},
    {"NodeSocketVectorXYZ", "NodeTreeInterfaceSocketVectorXYZ", SOCK_VECTOR, PROP_XYZ},
    {"NodeSocketColor", "NodeTreeInterfaceSocketColor", SOCK_RGBA, PROP_NONE},
    {"NodeSocketString", "NodeTreeInterfaceSocketString", SOCK_STRING, PROP_NONE},
    {"NodeSocketShader", "NodeTreeInterfaceSocketShader", SOCK_SHADER, PROP_NONE},
    {"NodeSocketObject", "NodeTreeInterfaceSocketObject", SOCK_OBJECT, PROP_NONE},
    {"NodeSocketImage", "NodeTreeInterfaceSocketImage", SOCK_IMAGE, PROP_NONE},
    {"NodeSocketGeometry", "NodeTreeInterfaceSocketGeometry", SOCK_GEOMETRY, PROP_NONE},
    {"NodeSocketCollection", "NodeTreeInterfaceSocketCollection", SOCK_COLLECTION, PROP_NONE},
    {"NodeSocketTexture", "NodeTreeInterfaceSocketTexture", SOCK_TEXTURE, PROP_NONE},
    {"NodeSocketMaterial", "NodeTreeInterfaceSocketMaterial", SOCK_MATERIAL, PROP_NONE},
};

template<typename Fn> bool socket_data_to_static_type(const eNodeSocketDatatype type, const Fn &fn)
{
  switch (type) {
    case SOCK_FLOAT:
      fn.template operator()<bNodeSocketValueFloat>();
      return true;
    case SOCK_INT:
      fn.template operator()<bNodeSocketValueInt>();
      return true;
    case SOCK_BOOLEAN:
      fn.template operator()<bNodeSocketValueBoolean>();
      return true;
    case SOCK_ROTATION:
      fn.template operator()<bNodeSocketValueRotation>();
      return true;
    case SOCK_VECTOR:
      fn.template operator()<bNodeSocketValueVector>();
      return true;
    case SOCK_RGBA:
      fn.template operator()<bNodeSocketValueRGBA>();
      return true;
    case SOCK_STRING:
      fn.template operator()<bNodeSocketValueString>();
      return true;
    case SOCK_OBJECT:
      fn.template operator()<bNodeSocketValueObject>();
      return true;
    case SOCK_IMAGE:
      fn.template operator()<bNodeSocketValueImage>();
      return true;
    case SOCK_COLLECTION:
      fn.template operator()<bNodeSocketValueCollection>();
      return true;
    case SOCK_TEXTURE:
      fn.template operator()<bNodeSocketValueTexture>();
      return true;
    case SOCK_MATERIAL:
      fn.template operator()<bNodeSocketValueMaterial>();
      return true;

    case SOCK_CUSTOM:
    case SOCK_SHADER:
    case SOCK_GEOMETRY:
      return true;
  }
  return false;
}

template<typename Fn> bool socket_data_to_static_type(const char *socket_type, const Fn &fn)
{
  for (const bNodeSocketStaticTypeInfo &info : node_socket_subtypes) {
    if (STREQ(socket_type, info.socket_identifier)) {
      return socket_data_to_static_type(info.type, fn);
    }
  }
  return false;
}

namespace detail {

template<typename Fn> struct TypeTagExecutor {
  const Fn &fn;

  TypeTagExecutor(const Fn &fn_) : fn(fn_) {}

  template<typename T> void operator()() const
  {
    fn(TypeTag<T>{});
  }
};

}  // namespace detail

template<typename Fn> void socket_data_to_static_type_tag(const char *socket_type, const Fn &fn)
{
  detail::TypeTagExecutor executor{fn};
  socket_data_to_static_type(socket_type, executor);
}

}  // namespace socket_types

template<typename T> bool socket_data_is_type(const char *socket_type)
{
  bool match = false;
  socket_types::socket_data_to_static_type_tag(socket_type, [&match](auto type_tag) {
    using SocketDataType = typename decltype(type_tag)::type;
    match |= std::is_same_v<T, SocketDataType>;
  });
  return match;
}

template<typename T> T &get_socket_data_as(bNodeTreeInterfaceSocket &item)
{
  BLI_assert(socket_data_is_type<T>(item.socket_type));
  return *static_cast<T *>(item.socket_data);
}

template<typename T> const T &get_socket_data_as(const bNodeTreeInterfaceSocket &item)
{
  BLI_assert(socket_data_is_type<T>(item.socket_type));
  return *static_cast<const T *>(item.socket_data);
}

inline bNodeTreeInterfaceSocket *add_interface_socket_from_node(bNodeTree &ntree,
                                                                const bNode &from_node,
                                                                const bNodeSocket &from_sock,
                                                                const StringRefNull socket_type,
                                                                const StringRefNull name)
{
  NodeTreeInterfaceSocketFlag flag = NodeTreeInterfaceSocketFlag(0);
  SET_FLAG_FROM_TEST(flag, from_sock.in_out & SOCK_IN, NODE_INTERFACE_SOCKET_INPUT);
  SET_FLAG_FROM_TEST(flag, from_sock.in_out & SOCK_OUT, NODE_INTERFACE_SOCKET_OUTPUT);

  bNodeTreeInterfaceSocket *iosock = ntree.tree_interface.add_socket(
      name.data(), from_sock.description, socket_type, flag, nullptr);
  if (iosock == nullptr) {
    return nullptr;
  }
  const bNodeSocketType *typeinfo = iosock->socket_typeinfo();
  if (typeinfo->interface_from_socket) {
    typeinfo->interface_from_socket(&ntree.id, iosock, &from_node, &from_sock);
    UNUSED_VARS(from_sock);
  }
  return iosock;
}

inline bNodeTreeInterfaceSocket *add_interface_socket_from_node(bNodeTree &ntree,
                                                                const bNode &from_node,
                                                                const bNodeSocket &from_sock,
                                                                const StringRefNull socket_type)
{
  return add_interface_socket_from_node(ntree, from_node, from_sock, socket_type, from_sock.name);
}

inline bNodeTreeInterfaceSocket *add_interface_socket_from_node(bNodeTree &ntree,
                                                                const bNode &from_node,
                                                                const bNodeSocket &from_sock)
{
  return add_interface_socket_from_node(
      ntree, from_node, from_sock, from_sock.typeinfo->idname, from_sock.name);
}

}  // namespace node_interface

}  // namespace blender::bke
