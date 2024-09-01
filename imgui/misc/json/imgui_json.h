# ifndef __IMGUI_JSON_H__
# define __IMGUI_JSON_H__
# include <imgui.h>
# include <type_traits>
# include <string>
# include <vector>
# include <map>
# include <cstddef>
# include <cstdint>
# include <algorithm>
# include <sstream>

# ifndef JSON_ASSERT
#     include <cassert>
#     define JSON_ASSERT(expr) assert(expr)
# endif

# ifndef JSON_IO
#     define JSON_IO 1
# endif

namespace imgui_json {

struct value;

using string  = std::string;
using object  = std::map<string, value>;
using array   = std::vector<value>;
using number  = double;
using boolean = bool;
using null    = std::nullptr_t;
using point   = std::intptr_t;
using vec2    = ImVec2;
using vec4    = ImVec4;

enum class type_t
{
    null,
    object,
    array,
    string,
    boolean,
    number,
    point,
    vec2,
    vec4,
    discarded
};

struct IMGUI_API value
{
    value(type_t type = type_t::null): m_Type(construct(m_Storage, type)) {}
    value(value&& other);
    value(const value& other);

    value(      null)      : m_Type(construct(m_Storage,      null()))  {}
    value(      object&& v): m_Type(construct(m_Storage, std::move(v))) {}
    value(const object&  v): m_Type(construct(m_Storage,           v))  {}
    value(      array&&  v): m_Type(construct(m_Storage, std::move(v))) {}
    value(const array&   v): m_Type(construct(m_Storage,           v))  {}
    value(      string&& v): m_Type(construct(m_Storage, std::move(v))) {}
    value(const string&  v): m_Type(construct(m_Storage,           v))  {}
    value(const char*    v): m_Type(construct(m_Storage,           v))  {}
    value(const point    v): m_Type(construct(m_Storage,           v))  {}
    value(      boolean  v): m_Type(construct(m_Storage,           v))  {}
    value(      number   v): m_Type(construct(m_Storage,           v))  {}
    value(      vec2&&   v): m_Type(construct(m_Storage, std::move(v))) {}
    value(const vec2&    v): m_Type(construct(m_Storage,           v))  {}
    value(      vec4&&   v): m_Type(construct(m_Storage, std::move(v))) {}
    value(const vec4&    v): m_Type(construct(m_Storage,           v))  {}
    ~value() { destruct(m_Storage, m_Type); }

    value& operator=(value&& other)      { if (this != &other) { value(std::move(other)).swap(*this); } return *this; }
    value& operator=(const value& other) { if (this != &other) { value(          other).swap(*this);  } return *this; }

    value& operator=(      null)       { auto other = value(           );  swap(other); return *this; }
    value& operator=(      object&& v) { auto other = value(std::move(v)); swap(other); return *this; }
    value& operator=(const object&  v) { auto other = value(          v);  swap(other); return *this; }
    value& operator=(      array&&  v) { auto other = value(std::move(v)); swap(other); return *this; }
    value& operator=(const array&   v) { auto other = value(          v);  swap(other); return *this; }
    value& operator=(      string&& v) { auto other = value(std::move(v)); swap(other); return *this; }
    value& operator=(const string&  v) { auto other = value(          v);  swap(other); return *this; }
    value& operator=(const char*    v) { auto other = value(          v);  swap(other); return *this; }
    value& operator=(const point    v) { auto other = value(          v);  swap(other); return *this; }
    value& operator=(      boolean  v) { auto other = value(          v);  swap(other); return *this; }
    value& operator=(      number   v) { auto other = value(          v);  swap(other); return *this; }
    value& operator=(      vec2&&   v) { auto other = value(std::move(v)); swap(other); return *this; }
    value& operator=(const vec2&    v) { auto other = value(          v);  swap(other); return *this; }
    value& operator=(      vec4&&   v) { auto other = value(std::move(v)); swap(other); return *this; }
    value& operator=(const vec4&    v) { auto other = value(          v);  swap(other); return *this; }

    type_t type() const { return m_Type; }

    operator type_t() const { return m_Type; }

            value& operator[](size_t index);
    const   value& operator[](size_t index) const;
            value& operator[](const string& key);
    const   value& operator[](const string& key) const;

    bool contains(const string& key) const;

    void push_back(const value& value);
    void push_back(value&& value);

    size_t erase(const string& key);

    bool is_primitive()  const { return is_string() || is_number() || is_boolean() || is_null() || is_point(); }
    bool is_structured() const { return is_object() || is_array();   }
    bool is_null()       const { return m_Type == type_t::null;      }
    bool is_object()     const { return m_Type == type_t::object;    }
    bool is_array()      const { return m_Type == type_t::array;     }
    bool is_string()     const { return m_Type == type_t::string;    }
    bool is_boolean()    const { return m_Type == type_t::boolean;   }
    bool is_number()     const { return m_Type == type_t::number;    }
    bool is_point()      const { return m_Type == type_t::point;     }
    bool is_discarded()  const { return m_Type == type_t::discarded; }
    bool is_vec2()       const { return m_Type == type_t::vec2;      }
    bool is_vec4()       const { return m_Type == type_t::vec4;      }

    template <typename T> const T& get() const;
    template <typename T>       T& get();

    template <typename T> const T* get_ptr() const;
    template <typename T>       T* get_ptr();

    string dump(const int indent = -1, const char indent_char = ' ') const;

    void swap(value& other);

    inline friend void swap(value& lhs, value& rhs) { lhs.swap(rhs); }

    // Returns discarded value for invalid inputs.
    static value parse(const string& data);

# if JSON_IO
    static std::pair<value, bool> load(const string& path);
    bool save(const string& path, const int indent = 4, const char indent_char = ' ') const;
# endif

private:
    struct parser;

    // VS2015: std::max() is not constexpr yet.
# define JSON_MAX2(a, b)                ((a) < (b) ? (b) : (a))
# define JSON_MAX3(a, b, c)             JSON_MAX2(JSON_MAX2(a, b), c)
# define JSON_MAX4(a, b, c, d)          JSON_MAX2(JSON_MAX3(a, b, c), d)
# define JSON_MAX5(a, b, c, d, e)       JSON_MAX2(JSON_MAX4(a, b, c, d), e)
# define JSON_MAX6(a, b, c, d, e, f)    JSON_MAX2(JSON_MAX5(a, b, c, d, e), f)
    enum
    {
        max_size  = JSON_MAX5( sizeof(string),  sizeof(object),  sizeof(array),  sizeof(number),  sizeof(boolean)),
        max_align = JSON_MAX5(alignof(string), alignof(object), alignof(array), alignof(number), alignof(boolean))
    };
# undef JSON_MAX6
# undef JSON_MAX5
# undef JSON_MAX4
# undef JSON_MAX3
# undef JSON_MAX2
    struct storage_t { alignas(max_align) unsigned char data[max_size]; };

    static       object*   object_ptr(      storage_t& storage) { return reinterpret_cast<       object*>(storage.data); }
    static const object*   object_ptr(const storage_t& storage) { return reinterpret_cast<const  object*>(storage.data); }
    static       array*     array_ptr(      storage_t& storage) { return reinterpret_cast<        array*>(storage.data); }
    static const array*     array_ptr(const storage_t& storage) { return reinterpret_cast<const   array*>(storage.data); }
    static       string*   string_ptr(      storage_t& storage) { return reinterpret_cast<       string*>(storage.data); }
    static const string*   string_ptr(const storage_t& storage) { return reinterpret_cast<const  string*>(storage.data); }
    static       boolean* boolean_ptr(      storage_t& storage) { return reinterpret_cast<      boolean*>(storage.data); }
    static const boolean* boolean_ptr(const storage_t& storage) { return reinterpret_cast<const boolean*>(storage.data); }
    static       number*   number_ptr(      storage_t& storage) { return reinterpret_cast<       number*>(storage.data); }
    static const number*   number_ptr(const storage_t& storage) { return reinterpret_cast<const  number*>(storage.data); }
    static       point*     point_ptr(      storage_t& storage) { return reinterpret_cast<        point*>(storage.data); }
    static const point*     point_ptr(const storage_t& storage) { return reinterpret_cast<const   point*>(storage.data); }
    static       vec2*       vec2_ptr(      storage_t& storage) { return reinterpret_cast<         vec2*>(storage.data); }
    static const vec2*       vec2_ptr(const storage_t& storage) { return reinterpret_cast<const    vec2*>(storage.data); }
    static       vec4*       vec4_ptr(      storage_t& storage) { return reinterpret_cast<         vec4*>(storage.data); }
    static const vec4*       vec4_ptr(const storage_t& storage) { return reinterpret_cast<const    vec4*>(storage.data); }

    static type_t construct(storage_t& storage, type_t type)
    {
        switch (type)
        {
            case type_t::object:    new (storage.data) object();  break;
            case type_t::array:     new (storage.data) array();   break;
            case type_t::string:    new (storage.data) string();  break;
            case type_t::boolean:   new (storage.data) boolean(); break;
            case type_t::number:    new (storage.data) number();  break;
            case type_t::point:     new (storage.data) point();   break;
            case type_t::vec2:      new (storage.data) vec2();    break;
            case type_t::vec4:      new (storage.data) vec4();    break;
            default: break;
        }

        return type;
    }

    static type_t construct(storage_t& storage,       null)           { (void)storage;                                        return type_t::null;    }
    static type_t construct(storage_t& storage,       object&& value) { new (storage.data)  object(std::forward<object>(value));  return type_t::object;  }
    static type_t construct(storage_t& storage, const object&  value) { new (storage.data)  object(value);                        return type_t::object;  }
    static type_t construct(storage_t& storage,       array&&  value) { new (storage.data)   array(std::forward<array>(value));   return type_t::array;   }
    static type_t construct(storage_t& storage, const array&   value) { new (storage.data)   array(value);                        return type_t::array;   }
    static type_t construct(storage_t& storage,       string&& value) { new (storage.data)  string(std::forward<string>(value));  return type_t::string;  }
    static type_t construct(storage_t& storage, const string&  value) { new (storage.data)  string(value);                        return type_t::string;  }
    static type_t construct(storage_t& storage, const char*    value) { new (storage.data)  string(value);                        return type_t::string;  }
    static type_t construct(storage_t& storage,       boolean  value) { new (storage.data) boolean(value);                        return type_t::boolean; }
    static type_t construct(storage_t& storage,       number   value) { new (storage.data)  number(value);                        return type_t::number;  }
    static type_t construct(storage_t& storage,       point    value) { new (storage.data)   point(value);                        return type_t::point;   }
    static type_t construct(storage_t& storage,       vec2&&   value) { new (storage.data)    vec2(std::forward<vec2>(value));    return type_t::vec2;    }
    static type_t construct(storage_t& storage, const vec2&    value) { new (storage.data)    vec2(value);                        return type_t::vec2;    }
    static type_t construct(storage_t& storage,       vec4&&   value) { new (storage.data)    vec4(std::forward<vec4>(value));    return type_t::vec4;    }
    static type_t construct(storage_t& storage, const vec4&    value) { new (storage.data)    vec4(value);                        return type_t::vec4;    }

    static void destruct(storage_t& storage, type_t type)
    {
        switch (type)
        {
            case type_t::object: object_ptr(storage)->~object(); break;
            case type_t::array:   array_ptr(storage)->~array();  break;
            case type_t::string: string_ptr(storage)->~string(); break;
            case type_t::vec2:     vec2_ptr(storage)->~vec2(); break;
            case type_t::vec4:     vec4_ptr(storage)->~vec4(); break;
            default: break;
        }
    }

    struct dump_context_t
    {
        std::ostringstream out;
        const int  indent = -1;
        const char indent_char = ' ';

        // VS2015: Aggregate initialization isn't a thing yet.
        dump_context_t(const int indent, const char indent_char)
            : indent(indent)
            , indent_char(indent_char)
        {
        }

        void write_indent(int level);
        void write_separator();
        void write_newline();
    };

    void dump(dump_context_t& context, int level) const;

    storage_t m_Storage;
    type_t    m_Type;
};

template <> inline const object&  value::get<object>()  const { JSON_ASSERT(m_Type == type_t::object);  return *object_ptr(m_Storage);  }
template <> inline const array&   value::get<array>()   const { JSON_ASSERT(m_Type == type_t::array);   return *array_ptr(m_Storage);   }
template <> inline const string&  value::get<string>()  const { JSON_ASSERT(m_Type == type_t::string);  return *string_ptr(m_Storage);  }
template <> inline const boolean& value::get<boolean>() const { JSON_ASSERT(m_Type == type_t::boolean); return *boolean_ptr(m_Storage); }
template <> inline const number&  value::get<number>()  const { JSON_ASSERT(m_Type == type_t::number);  return *number_ptr(m_Storage);  }
template <> inline const point&   value::get<point>()   const { JSON_ASSERT(m_Type == type_t::point);   return *point_ptr(m_Storage);   }
template <> inline const vec2&    value::get<vec2>()    const { JSON_ASSERT(m_Type == type_t::vec2);    return *vec2_ptr(m_Storage);    }
template <> inline const vec4&    value::get<vec4>()    const { JSON_ASSERT(m_Type == type_t::vec4);    return *vec4_ptr(m_Storage);    }

template <> inline       object&  value::get<object>()        { JSON_ASSERT(m_Type == type_t::object);  return *object_ptr(m_Storage);  }
template <> inline       array&   value::get<array>()         { JSON_ASSERT(m_Type == type_t::array);   return *array_ptr(m_Storage);   }
template <> inline       string&  value::get<string>()        { JSON_ASSERT(m_Type == type_t::string);  return *string_ptr(m_Storage);  }
template <> inline       boolean& value::get<boolean>()       { JSON_ASSERT(m_Type == type_t::boolean); return *boolean_ptr(m_Storage); }
template <> inline       number&  value::get<number>()        { JSON_ASSERT(m_Type == type_t::number);  return *number_ptr(m_Storage);  }
template <> inline       point&   value::get<point>()         { JSON_ASSERT(m_Type == type_t::point);   return *point_ptr(m_Storage);   }
template <> inline       vec2&    value::get<vec2>()          { JSON_ASSERT(m_Type == type_t::vec2);    return *vec2_ptr(m_Storage);    }
template <> inline       vec4&    value::get<vec4>()          { JSON_ASSERT(m_Type == type_t::vec4);    return *vec4_ptr(m_Storage);    }

template <> inline const object*  value::get_ptr<object>()  const { if (m_Type == type_t::object)  return object_ptr(m_Storage);  else return nullptr; }
template <> inline const array*   value::get_ptr<array>()   const { if (m_Type == type_t::array)   return array_ptr(m_Storage);   else return nullptr; }
template <> inline const string*  value::get_ptr<string>()  const { if (m_Type == type_t::string)  return string_ptr(m_Storage);  else return nullptr; }
template <> inline const boolean* value::get_ptr<boolean>() const { if (m_Type == type_t::boolean) return boolean_ptr(m_Storage); else return nullptr; }
template <> inline const number*  value::get_ptr<number>()  const { if (m_Type == type_t::number)  return number_ptr(m_Storage);  else return nullptr; }
template <> inline const point*   value::get_ptr<point>()   const { if (m_Type == type_t::point)   return point_ptr(m_Storage);   else return nullptr; }
template <> inline const vec2*    value::get_ptr<vec2>()    const { if (m_Type == type_t::vec2)    return vec2_ptr(m_Storage);    else return nullptr; }
template <> inline const vec4*    value::get_ptr<vec4>()    const { if (m_Type == type_t::vec4)    return vec4_ptr(m_Storage);    else return nullptr; }

template <> inline       object*  value::get_ptr<object>()        { if (m_Type == type_t::object)  return object_ptr(m_Storage);  else return nullptr; }
template <> inline       array*   value::get_ptr<array>()         { if (m_Type == type_t::array)   return array_ptr(m_Storage);   else return nullptr; }
template <> inline       string*  value::get_ptr<string>()        { if (m_Type == type_t::string)  return string_ptr(m_Storage);  else return nullptr; }
template <> inline       boolean* value::get_ptr<boolean>()       { if (m_Type == type_t::boolean) return boolean_ptr(m_Storage); else return nullptr; }
template <> inline       number*  value::get_ptr<number>()        { if (m_Type == type_t::number)  return number_ptr(m_Storage);  else return nullptr; }
template <> inline       point*   value::get_ptr<point>()         { if (m_Type == type_t::point)   return point_ptr(m_Storage);   else return nullptr; }
template <> inline       vec2*    value::get_ptr<vec2>()          { if (m_Type == type_t::vec2)    return vec2_ptr(m_Storage);    else return nullptr; }
template <> inline       vec4*    value::get_ptr<vec4>()          { if (m_Type == type_t::vec4)    return vec4_ptr(m_Storage);    else return nullptr; }

template <typename T>
inline bool GetPtrTo(const imgui_json::value& value, std::string key, const T*& result)
{
    if (!value.contains(key))
        return false;
    auto& valueObject = value[key];
    auto valuePtr = valueObject.get_ptr<T>();
    if (valuePtr == nullptr)
        return false;
    result = valuePtr;
    return true;
};

template <typename T, typename V>
inline bool GetTo(const imgui_json::value& value, std::string key, V& result)
{
    const T* valuePtr = nullptr;
    if (!GetPtrTo(value, key, valuePtr))
        return false;
    result = static_cast<V>(*valuePtr);
    return true;
};

} // namespace imgui_json

# endif // __IMGUI_JSON_H__