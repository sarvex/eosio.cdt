#pragma once
#include <eosio/datastream.hpp>
#include <eosio/name.hpp>
#include <eosio/varint.hpp>

#include <eosio/to_key.hpp>

#include <algorithm>
#include <cctype>
#include <functional>

#define EOSIO_CDT_GET_RETURN_T(value_class, index_name) std::decay_t<decltype(std::invoke(&value_class::index_name, std::declval<const value_class*>()))>

/**
 * @brief Macro to define an index.
 * @details In the case where the autogenerated index names created by DEFINE_TABLE are not enough, a user can instead
 * manually define the table and indices. This macro allows users to conveniently define an index without having to specify
 * the index template type, as those can be large/unwieldy to type out.
 *
 * @param index_name    - The index name.
 * @param member_name   - The name of the member pointer used for the index. This also defines the index's C++ variable name.
 */
#define KV_NAMED_INDEX(index_name, member_name)                                                                        \
   index<EOSIO_CDT_GET_RETURN_T(value_type, member_name)> member_name{eosio::name{index_name}, &value_type::member_name};

namespace eosio {
   namespace internal_use_do_not_use {
      extern "C" {
         __attribute__((eosio_wasm_import))
         int64_t kv_erase(uint64_t db, uint64_t contract, const char* key, uint32_t key_size);

         __attribute__((eosio_wasm_import))
         int64_t kv_set(uint64_t db, uint64_t contract, const char* key, uint32_t key_size, const char* value, uint32_t value_size);

         __attribute__((eosio_wasm_import))
         bool kv_get(uint64_t db, uint64_t contract, const char* key, uint32_t key_size, uint32_t& value_size);

         __attribute__((eosio_wasm_import))
         uint32_t kv_get_data(uint64_t db, uint32_t offset, char* data, uint32_t data_size);

         __attribute__((eosio_wasm_import))
         uint32_t kv_it_create(uint64_t db, uint64_t contract, const char* prefix, uint32_t size);

         __attribute__((eosio_wasm_import))
         void kv_it_destroy(uint32_t itr);

         __attribute__((eosio_wasm_import))
         int32_t kv_it_status(uint32_t itr);

         __attribute__((eosio_wasm_import))
         int32_t kv_it_compare(uint32_t itr_a, uint32_t itr_b);

         __attribute__((eosio_wasm_import))
         int32_t kv_it_key_compare(uint32_t itr, const char* key, uint32_t size);

         __attribute__((eosio_wasm_import))
         int32_t kv_it_move_to_end(uint32_t itr);

         __attribute__((eosio_wasm_import))
         int32_t kv_it_next(uint32_t itr);

         __attribute__((eosio_wasm_import))
         int32_t kv_it_prev(uint32_t itr);

         __attribute__((eosio_wasm_import))
         int32_t kv_it_lower_bound(uint32_t itr, const char* key, uint32_t size);

         __attribute__((eosio_wasm_import))
         int32_t kv_it_key(uint32_t itr, uint32_t offset, char* dest, uint32_t size, uint32_t& actual_size);

         __attribute__((eosio_wasm_import))
         int32_t kv_it_value(uint32_t itr, uint32_t offset, char* dest, uint32_t size, uint32_t& actual_size);
      }
   }

namespace detail {
   constexpr inline size_t max_stack_buffer_size = 512;
}

/**
 * The key_type struct is used to store the binary representation of a key.
 */
struct key_type : private std::vector<char> {
   key_type() = default;

   explicit key_type(std::vector<char>&& v) : std::vector<char>(v) {}

   key_type(char* str, size_t size) : std::vector<char>(str, str+size) {}

   key_type operator+(const key_type& b) const {
      key_type ret = *this;
      ret += b;
      return ret;
   }

   key_type& operator+=(const key_type& b) {
      this->insert(this->end(), b.begin(), b.end());
      return *this;
   }

   static key_type from_hex( const std::string_view& str ) {
      key_type out;

      check( str.size() % 2 == 0, "invalid hex string length" );
      out.reserve( str.size() / 2 );

      auto start = str.data();
      auto end   = start + str.size();
      for(const char* p = start; p != end; p+=2 ) {
          auto hic = p[0];
          auto lowc = p[1];

          uint8_t hi  = hic  <= '9' ? hic-'0' : 10+(hic-'a');
          uint8_t low = lowc <= '9' ? lowc-'0' : 10+(lowc-'a');

          out.push_back( char((hi << 4) | low) );
      }

      return out;
   }

   std::string to_hex() const {
      const char* hex_characters = "0123456789abcdef";

      uint32_t buffer_size = 2 * size();
      check(buffer_size >= size(), "length passed into printhex is too large");

      void* buffer = buffer_size > detail::max_stack_buffer_size ? malloc(buffer_size) : alloca(buffer_size);

      char* b = reinterpret_cast<char*>(buffer);
      const uint8_t* d = reinterpret_cast<const uint8_t*>(data());
      for(uint32_t i = 0; i < size(); ++i) {
         *b = hex_characters[d[i] >> 4];
         ++b;
         *b = hex_characters[d[i] & 0x0f];
         ++b;
      }

      std::string ret{reinterpret_cast<char*>(buffer), buffer_size};

      if (buffer_size > detail::max_stack_buffer_size) {
         free(buffer);
      }

      return ret;
   }

   using std::vector<char>::data;
   using std::vector<char>::size;
   using std::vector<char>::resize;
};

/* @cond PRIVATE */
template <typename T>
inline key_type make_key(T&& t) {
   auto bytes = convert_to_key(std::forward<T>(t));
   eosio::check((bool)bytes, "There was a failure in make_key."); 
   return key_type(std::move(bytes.value()));
}

inline key_type make_prefix(eosio::name table_name, eosio::name index_name, uint8_t status = 1) {
   return make_key(std::make_tuple(status, table_name, index_name));
}

inline key_type table_key(const key_type& prefix, const key_type& key) {
   return prefix + key;
}
/* @endcond */

// This is the "best" way to document a function that does not technically exist using Doxygen.
#if EOSIO_CDT_DOXYGEN
/**
 * @brief A function for converting types to the appropriate binary representation for the EOSIO Key Value database.
 * @details The CDT provides implementations of this function for many of the common primitives and for structs/tuples.
 * If sticking with standard types, contract developers should not need to interact with this function.
 * If doing something more advanced, contract developers may need to provide their own implementation for a special type.
 */
template <typename T>
inline key_type make_key(T val) {
   return {};
}
#endif

static constexpr eosio::name kv_ram = "eosio.kvram"_n;
static constexpr eosio::name kv_disk = "eosio.kvdisk"_n;

template<typename T>
class kv_table;

namespace kv_detail {

   class kv_index {

   public:

      eosio::name index_name;
      eosio::name table_name;
      eosio::name contract_name;

      key_type to_table_key( const key_type& k )const{ return table_key( prefix, k ); }

   protected:
      kv_index() = default;

      template <typename KF, typename T>
      kv_index(eosio::name index_name, KF&& kf, T*) : index_name{index_name} {
         key_function = [=](const void* t) {
            return make_key(std::invoke(kf, static_cast<const T*>(t)));
         };
      }

      template<typename T>
      key_type get_key(const T& inst) const { return key_function(&inst); }
      key_type get_key_void(const void* ptr) const { return key_function(ptr); }
      void* tbl;
      key_type prefix;

   private:
      template<typename T>
      friend class eosio::kv_table;
      friend class kv_table_base;

      std::function<key_type(const void*)> key_function;

      virtual void setup() = 0;
   };

   class kv_table_base {
    protected:
      eosio::name contract_name;
      eosio::name table_name;
      uint64_t db_name;

      eosio::name primary_index_name;

      kv_index* primary_index;
      std::vector<kv_index*> secondary_indices;

      void put_secondary(const key_type& tbl_key, const void* value, const void* old_value) {
         for (const auto& idx : secondary_indices) {
            uint32_t value_size;
            auto sec_tbl_key = table_key(make_prefix(table_name, idx->index_name), idx->get_key_void(value));
            auto sec_found = internal_use_do_not_use::kv_get(db_name, contract_name.value, sec_tbl_key.data(), sec_tbl_key.size(), value_size);

            if (!old_value) {
               eosio::check(!sec_found, "Attempted to store an existing secondary index.");
               internal_use_do_not_use::kv_set(db_name, contract_name.value, sec_tbl_key.data(), sec_tbl_key.size(), tbl_key.data(), tbl_key.size());
            } else {
               if (sec_found) {
                  void* buffer = value_size > detail::max_stack_buffer_size ? malloc(value_size) : alloca(value_size);
                  auto copy_size = internal_use_do_not_use::kv_get_data(db_name, 0, (char*)buffer, value_size);

                  auto res = memcmp(buffer, tbl_key.data(), copy_size);
                  eosio::check(copy_size == tbl_key.size() && res == 0, "Attempted to update an existing secondary index.");

                  if (copy_size > detail::max_stack_buffer_size) {
                     free(buffer);
                  }
               } else {
                  auto old_sec_key = table_key(make_prefix(table_name, idx->index_name), idx->get_key_void(old_value));
                  internal_use_do_not_use::kv_erase(db_name, contract_name.value, old_sec_key.data(), old_sec_key.size());
                  internal_use_do_not_use::kv_set(db_name, contract_name.value, sec_tbl_key.data(), sec_tbl_key.size(), tbl_key.data(), tbl_key.size());
               }
            }
         }
      }
   };
}

/**
 * @defgroup keyvalue Key Value Table
 * @ingroup contracts
 *
 * @brief Defines an EOSIO Key Value Table
 * @details EOSIO Key Value API provides a C++ interface to the EOSIO Key Value database.
 * Key Value Tables require 1 primary index, of any type that can be serialized to a binary representation.
 * Key Value Tables support 0 or more secondary index, of any type that can be serialized to a binary representation.
 * Indexes must be a member variable or a member function.
 *
 * @tparam T         - the type of the data stored as the value of the table
  */
template<typename T>
class kv_table : kv_detail::kv_table_base {

   using kv_index = kv_detail::kv_index;

   class base_iterator {
   public:
      enum class status {
         iterator_ok     = 0,  // Iterator is positioned at a key-value pair
         iterator_erased = -1, // The key-value pair that the iterator used to be positioned at was erased
         iterator_end    = -2, // Iterator is out-of-bounds
      };

      base_iterator() = default;

      base_iterator(uint32_t itr, status itr_stat, const kv_index* index) : itr{itr}, itr_stat{itr_stat}, index{index} {}

      base_iterator(base_iterator&& other) :
         itr(std::exchange(other.itr, 0)),
         itr_stat(std::move(other.itr_stat))
      {}

      ~base_iterator() {
         if (itr) {
            internal_use_do_not_use::kv_it_destroy(itr);
         }
      }

      base_iterator& operator=(base_iterator&& other) {
         if (itr) {
            internal_use_do_not_use::kv_it_destroy(itr);
         }
         itr = std::exchange(other.itr, 0);
         itr_stat = std::move(other.itr_stat);
         return *this;
      }

      bool good()const { return itr_stat != status::iterator_end; }

      /**
       * Returns the value that the iterator points to.
       * @ingroup keyvalue
       *
       * @return The value that the iterator points to.
       */
      T value() const {
         using namespace detail;

         eosio::check(itr_stat != status::iterator_end, "Cannot read end iterator");

         uint32_t value_size;
         uint32_t actual_value_size;
         uint32_t actual_data_size;
         uint32_t offset = 0;

         // call once to get the value_size
         internal_use_do_not_use::kv_it_value(itr, 0, (char*)nullptr, 0, value_size);

         void* buffer = value_size > detail::max_stack_buffer_size ? malloc(value_size) : alloca(value_size);
         auto stat = internal_use_do_not_use::kv_it_value(itr, offset, (char*)buffer, value_size, actual_value_size);

         eosio::check(static_cast<status>(stat) == status::iterator_ok, "Error reading value");

         void* deserialize_buffer = buffer;
         size_t deserialize_size = actual_value_size;

         bool is_primary = index->index_name == static_cast<kv_table*>(index->tbl)->primary_index_name;
         if (!is_primary) {
            auto success = internal_use_do_not_use::kv_get(static_cast<kv_table*>(index->tbl)->db_name, index->contract_name.value, (char*)buffer, actual_value_size, actual_data_size);
            eosio::check(success, "failure getting primary key in `value()`");

            void* pk_buffer = actual_data_size > detail::max_stack_buffer_size ? malloc(actual_data_size) : alloca(actual_data_size);
            internal_use_do_not_use::kv_get_data(static_cast<kv_table*>(index->tbl)->db_name, 0, (char*)pk_buffer, actual_data_size);

            deserialize_buffer = pk_buffer;
            deserialize_size = actual_data_size;
         }

         T val;
         deserialize(val, deserialize_buffer, deserialize_size);

         if (value_size > detail::max_stack_buffer_size) {
            free(buffer);
         }

         if (is_primary && actual_data_size > detail::max_stack_buffer_size) {
            free(deserialize_buffer);
         }
         return val;
      }

      key_type key() const {
         uint32_t actual_value_size;
         uint32_t value_size;

         // call once to get the value size
         internal_use_do_not_use::kv_it_key(itr, 0, (char*)nullptr, 0, value_size);

         void* buffer = value_size > detail::max_stack_buffer_size ? malloc(value_size) : alloca(value_size);
         auto stat = internal_use_do_not_use::kv_it_key(itr, 0, (char*)buffer, value_size, actual_value_size);

         eosio::check(static_cast<status>(stat) == status::iterator_ok, "Error getting key");

         return {(char*)buffer, actual_value_size};
      }

   protected:
      uint32_t itr;
      status itr_stat;

      const kv_index* index;

      int compare(const base_iterator& b) const {
         bool a_is_end = !itr || itr_stat == status::iterator_end;
         bool b_is_end = !b.itr || b.itr_stat == status::iterator_end;
         if (a_is_end && b_is_end) {
            return 0;
         } else if (a_is_end && b.itr) {
            return 1;
         } else if (itr && b_is_end) {
            return -1;
         } else {
            return internal_use_do_not_use::kv_it_compare(itr, b.itr);
         }
      }
   };

   class iterator : public base_iterator {
      using base_iterator::itr;
      using base_iterator::itr_stat;
      using base_iterator::index;

   public:
      using status = typename base_iterator::status;

      iterator() = default;

      iterator(uint32_t itr, status itr_stat, const kv_index* index) : base_iterator{itr, itr_stat, index} {}

      iterator(iterator&& other) : base_iterator{std::move(other)} {}

      iterator& operator=(iterator&& other) {
         if (itr) {
            internal_use_do_not_use::kv_it_destroy(itr);
         }
         itr = std::exchange(other.itr, 0);
         itr_stat = std::move(other.itr_stat);
         index = std::move(other.index);
         return *this;
      }

      iterator& operator++() {
         eosio::check(itr_stat != status::iterator_end, "cannot increment end iterator");
         itr_stat = static_cast<status>(internal_use_do_not_use::kv_it_next(itr));
         return *this;
      }

      iterator& operator--() {
         if (!itr) {
            itr = internal_use_do_not_use::kv_it_create(static_cast<kv_table*>(index->tbl)->db_name, index->contract_name.value, index->prefix.data(), index->prefix.size());
         }
         itr_stat = static_cast<status>(internal_use_do_not_use::kv_it_prev(itr));
         eosio::check(itr_stat != status::iterator_end, "decremented past the beginning");
         return *this;
      }

      int32_t key_compare(key_type kt) const {
         if (itr == 0 || itr_stat == status::iterator_end) {
            return 1;
         } else {
            return internal_use_do_not_use::kv_it_key_compare(itr, kt.data(), kt.size());
         }
      }

      bool operator==(const iterator& b) const {
         return base_iterator::compare(b) == 0;
      }

      bool operator!=(const iterator& b) const {
         return base_iterator::compare(b) != 0;
      }

      bool operator<(const iterator& b) const {
         return base_iterator::compare(b) < 0;
      }

      bool operator<=(const iterator& b) const {
         return base_iterator::compare(b) <= 0;
      }

      bool operator>(const iterator& b) const {
         return base_iterator::compare(b) > 0;
      }

      bool operator>=(const iterator& b) const {
         return base_iterator::compare(b) >= 0;
      }
   };

   class reverse_iterator : public base_iterator {
      using base_iterator::itr;
      using base_iterator::itr_stat;
      using base_iterator::index;

   public:
      using status = typename base_iterator::status;

      reverse_iterator() = default;

      reverse_iterator(uint32_t itr, status itr_stat, const kv_index* index) : base_iterator{itr, itr_stat, index} {}

      reverse_iterator(reverse_iterator&& other) : base_iterator{std::move(other)} {}

      reverse_iterator& operator=(reverse_iterator&& other) {
         if (itr) {
            internal_use_do_not_use::kv_it_destroy(itr);
         }
         itr = std::exchange(other.itr, 0);
         itr_stat = std::move(other.itr_stat);
         index = std::move(other.index);
         return *this;
      }

      reverse_iterator& operator++() {
         eosio::check(itr_stat != status::iterator_end, "incremented past the end");
         itr_stat = static_cast<status>(internal_use_do_not_use::kv_it_prev(itr));
         return *this;
      }

      reverse_iterator& operator--() {
         if (!itr) {
            itr = internal_use_do_not_use::kv_it_create(index->tbl->db_name, index->contract_name.value, index->prefix.data(), index->prefix.size());
            itr_stat = static_cast<status>(internal_use_do_not_use::kv_it_lower_bound(itr, "", 0));
         }
         itr_stat = static_cast<status>(internal_use_do_not_use::kv_it_next(itr));
         eosio::check(itr_stat != status::iterator_end, "decremented past the beginning");
         return *this;
      }

      int32_t key_compare(key_type kt) const {
         if (itr == 0 || itr_stat == status::iterator_end) {
            return 1;
         } else {
            return internal_use_do_not_use::kv_it_key_compare(itr, kt.data(), kt.size());
         }
      }

      bool operator==(const reverse_iterator& b) const {
         return base_iterator::compare(b) == 0;
      }

      bool operator!=(const reverse_iterator& b) const {
         return base_iterator::compare(b) != 0;
      }

      bool operator<(const reverse_iterator& b) const {
         return base_iterator::compare(b) < 0;
      }

      bool operator<=(const reverse_iterator& b) const {
         return base_iterator::compare(b) <= 0;
      }

      bool operator>(const reverse_iterator& b) const {
         return base_iterator::compare(b) > 0;
      }

      bool operator>=(const reverse_iterator& b) const {
         return base_iterator::compare(b) >= 0;
      }
   };

public:

   using iterator = kv_table::iterator;
   using value_type = T;

   /**
    * @ingroup keyvalue
    *
    * @brief Defines an index on an EOSIO Key Value Table
    * @details A Key Value Index allows a user of the table to search based on a given field.
    * The only restrictions on that field are that it is serializable to a binary representation sortable by the KV intrinsics.
    * Convenience functions exist to handle most of the primitive types as well as some more complex types, and are
    * used automatically where possible.
    *
    * @tparam K - The type of the key used in the index.
    */
   template <typename K>
   class index : public kv_index {
   public:
      using iterator = kv_table::iterator;
      using kv_table<T>::kv_index::tbl;
      using kv_table<T>::kv_index::table_name;
      using kv_table<T>::kv_index::contract_name;
      using kv_table<T>::kv_index::index_name;
      using kv_table<T>::kv_index::prefix;

      template <typename KF>
      index(eosio::name name, KF&& kf) : kv_index{name, kf, (T*)nullptr} {
         static_assert(std::is_same_v<K, std::remove_cv_t<std::decay_t<decltype(std::invoke(kf, std::declval<const T*>()))>>>,
               "Make sure the variable/function passed to the constructor returns the same type as the template parameter.");
      }

      /**
       * Search for an existing object in a table by the index, using the given key.
       * @ingroup keyvalue
       *
       * @param key - The key to search for.
       * @return An iterator to the found object OR the `end` iterator if the given key was not found.
       */
      iterator find(const K& key) const {
         auto t_key = table_key(prefix, make_key(key));

         return find(t_key);
      }

      iterator find(const key_type& key) const {
         uint32_t itr = internal_use_do_not_use::kv_it_create(static_cast<kv_table*>(tbl)->db_name, contract_name.value, prefix.data(), prefix.size());
         int32_t itr_stat = internal_use_do_not_use::kv_it_lower_bound(itr, key.data(), key.size());

         auto cmp = internal_use_do_not_use::kv_it_key_compare(itr, key.data(), key.size());

         if (cmp != 0) {
            internal_use_do_not_use::kv_it_destroy(itr);
            return end();
         }

         return {itr, static_cast<typename iterator::status>(itr_stat), this};
      }

      /**
       * Check if a given key exists in the index.
       * @ingroup keyvalue
       *
       * @param key - The key to check for.
       * @return If the key exists or not.
       */
      bool exists(const K& key) const {
         auto t_key = table_key(prefix, make_key(key));
         return exists(t_key);
      }

      bool exists(const key_type& key) const {
         uint32_t value_size;
         return internal_use_do_not_use::kv_get(static_cast<kv_table*>(tbl)->db_name, contract_name.value, key.data(), key.size(), value_size);
      }

      /**
       * Get the value for an existing object in a table by the index, using the given key.
       * @ingroup keyvalue
       *
       * @param key - The key to search for.
       * @return The value corresponding to the key.
       */
      T operator[](const K& key) const {
         return operator[](make_key(key));
      }

      T operator[](const key_type& key) const {
         auto opt = get(key);
         eosio::check(opt.has_value(), __FILE__ ":" + std::to_string(__LINE__) + " Key not found in `[]`");
         return *opt;
      }

      /**
       * Get the value for an existing object in a table by the index, using the given key.
       * @ingroup keyvalue
       *
       * @param key - The key to search for.
       * @return A std::optional of the value corresponding to the key.
       */
      std::optional<T> get(const K& key) const {
         return get(make_key(key));
      }

      std::optional<T> get(const key_type& k ) const {
         auto key =   table_key( prefix, k );
         uint32_t value_size;
         uint32_t actual_data_size;
         std::optional<T> ret_val;

         auto success = internal_use_do_not_use::kv_get(static_cast<kv_table*>(tbl)->db_name, contract_name.value, key.data(), key.size(), value_size);
         if (!success) {
            return ret_val;
         }

         void* buffer = value_size > detail::max_stack_buffer_size ? malloc(value_size) : alloca(value_size);
         auto copy_size = internal_use_do_not_use::kv_get_data(static_cast<kv_table*>(tbl)->db_name, 0, (char*)buffer, value_size);

         void* deserialize_buffer = buffer;
         size_t deserialize_size = copy_size;

         bool is_primary = index_name == static_cast<kv_table*>(tbl)->primary_index_name;
         if (!is_primary) {
            auto success = internal_use_do_not_use::kv_get(static_cast<kv_table*>(tbl)->db_name, contract_name.value, (char*)buffer, copy_size, actual_data_size);
            eosio::check(success, "failure getting primary key");

            void* pk_buffer = actual_data_size > detail::max_stack_buffer_size ? malloc(actual_data_size) : alloca(actual_data_size);
            auto pk_copy_size = internal_use_do_not_use::kv_get_data(static_cast<kv_table*>(tbl)->db_name, 0, (char*)pk_buffer, actual_data_size);

            deserialize_buffer = pk_buffer;
            deserialize_size = pk_copy_size;
         }

         ret_val.emplace();
         deserialize(*ret_val, deserialize_buffer, deserialize_size);

         if (value_size > detail::max_stack_buffer_size) {
            free(buffer);
         }

         if (is_primary && actual_data_size > detail::max_stack_buffer_size) {
            free(deserialize_buffer);
         }

         return ret_val;
      }

      /**
       * Returns an iterator to the object with the lowest key (by this index) in the table.
       * @ingroup keyvalue
       *
       * @return An iterator to the object with the lowest key (by this index) in the table.
       */
      iterator begin() const {
         uint32_t itr = internal_use_do_not_use::kv_it_create(static_cast<kv_table*>(tbl)->db_name, contract_name.value, prefix.data(), prefix.size());
         int32_t itr_stat = internal_use_do_not_use::kv_it_lower_bound(itr, "", 0);

         return {itr, static_cast<typename iterator::status>(itr_stat), this};
      }

      /**
       * Returns an iterator pointing past the end. It does not point to any element, therefore `value` should not be called on it.
       * @ingroup keyvalue
       *
       * @return An iterator pointing past the end.
       */
      iterator end() const {
         return {0, iterator::status::iterator_end, this};
      }

      /**
       * Returns a reverse iterator to the object with the highest key (by this index) in the table.
       * @ingroup keyvalue
       *
       * @return A reverse iterator to the object with the highest key (by this index) in the table.
       */
      reverse_iterator rbegin() const {
         uint32_t itr = internal_use_do_not_use::kv_it_create(static_cast<kv_table*>(tbl)->db_name, contract_name.value, prefix.data(), prefix.size());
         int32_t itr_stat = internal_use_do_not_use::kv_it_prev(itr);

         return {itr, static_cast<typename iterator::status>(itr_stat), this};
      }

      /**
       * Returns a reverse iterator pointing past the beginning. It does not point to any element, therefore `value` should not be called on it.
       * @ingroup keyvalue
       *
       * @return A reverse iterator pointing past the beginning.
       */
      reverse_iterator rend() const {
         return {0, iterator::status::iterator_end, this};
      }

      /**
       * Returns an iterator pointing to the element with the lowest key greater than or equal to the given key.
       * @ingroup keyvalue
       *
       * @return An iterator pointing to the element with the lowest key greater than or equal to the given key.
       */
      iterator lower_bound(const K& key) const {
         return lower_bound(make_key(key));
      }

      iterator lower_bound(const key_type& k ) const {
         auto key = table_key( prefix, k );
         uint32_t itr = internal_use_do_not_use::kv_it_create(static_cast<kv_table*>(tbl)->db_name, contract_name.value, prefix.data(), prefix.size());
         int32_t itr_stat = internal_use_do_not_use::kv_it_lower_bound(itr, key.data(), key.size());

         return {itr, static_cast<typename iterator::status>(itr_stat), this};
      }

      /**
       * Returns an iterator pointing to the first element greater than the given key.
       * @ingroup keyvalue
       *
       * @return An iterator pointing to the first element greater than the given key.
       */
      iterator upper_bound(const K& key) const {
         return upper_bound(make_key(key));
      }

      iterator upper_bound(const key_type& key) const {
         auto it = lower_bound(key);

         auto cmp = it.key_compare(key);
         if (cmp == 0) {
            ++it;
         }

         return it;
      }

      /**
       * Returns a vector of objects that fall between the specifed range. The range is inclusive, exclusive.
       * @ingroup keyvalue
       *
       * @param begin - The beginning of the range (inclusive).
       * @param end - The end of the range (exclusive).
       * @return A vector containing all the objects that fall between the range.
       */
      std::vector<T> range(const K& b, const K& e) const {
         return range(make_key(b), make_key(e));
      }

      std::vector<T> range(const key_type& b_key, const key_type& e_key) const {
         auto b = table_key(prefix, make_key(b_key));
         auto e = table_key(prefix, make_key(e_key));
         std::vector<T> return_values;

         for(auto itr = lower_bound(b), end_itr = lower_bound(e); itr < end_itr; ++itr) {
            return_values.push_back(itr.value());
         }

         return return_values;
      }

      void setup() override {
         prefix = make_prefix(table_name, index_name);
      }
   };

   /**
    * @ingroup keyvalue
    * Puts a value into the table. If the value already exists, it updates the existing entry.
    * The key is determined from the defined primary index.
    * If the put attempts to store over an existing secondary index, the transaction will be aborted.
    *
    * @param value - The entry to be stored in the table.
    */
   void put(const T& value) {
      uint32_t value_size;
      T old_value;

      auto primary_key = primary_index->get_key(value);
      auto tbl_key = table_key(make_prefix(table_name, primary_index->index_name), primary_key);

      auto primary_key_found = internal_use_do_not_use::kv_get(db_name, contract_name.value, tbl_key.data(), tbl_key.size(), value_size);

      if (primary_key_found) {
         void* buffer = value_size > detail::max_stack_buffer_size ? malloc(value_size) : alloca(value_size);
         auto copy_size = internal_use_do_not_use::kv_get_data(db_name, 0, (char*)buffer, value_size);

         deserialize(old_value, buffer, copy_size);

         if (value_size > detail::max_stack_buffer_size) {
            free(buffer);
         }
      }

      put_secondary(tbl_key, &value, primary_key_found ? &old_value : nullptr);

      size_t data_size = get_size(value);
      void* data_buffer = data_size > detail::max_stack_buffer_size ? malloc(data_size) : alloca(data_size);

      serialize(value, data_buffer, data_size);

      internal_use_do_not_use::kv_set(db_name, contract_name.value, tbl_key.data(), tbl_key.size(), (const char*)data_buffer, data_size);

      if (data_size > detail::max_stack_buffer_size) {
         free(data_buffer);
      }
   }

   /**
    * Removes a value from the table.
    * @ingroup keyvalue
    *
    * @param key - The key of the value to be removed.
    */
   void erase(const T& value) {
      uint32_t value_size;

      auto primary_key = primary_index->get_key(value);
      auto tbl_key = table_key(make_prefix(table_name, primary_index->index_name), primary_key);
      auto primary_key_found = internal_use_do_not_use::kv_get(db_name, contract_name.value, tbl_key.data(), tbl_key.size(), value_size);

      if (!primary_key_found) {
         return;
      }

      for (const auto& idx : secondary_indices) {
         auto sec_tbl_key = table_key(make_prefix(table_name, idx->index_name), idx->get_key(value));
         internal_use_do_not_use::kv_erase(db_name, contract_name.value, sec_tbl_key.data(), sec_tbl_key.size());
      }

      internal_use_do_not_use::kv_erase(db_name, contract_name.value, tbl_key.data(), tbl_key.size());
   }

protected:
   kv_table() = default;

   template <typename I>
   void setup_indices(I& index) {
      kv_index* idx = &index;
      idx->contract_name = contract_name;
      idx->table_name = table_name;
      idx->tbl = this;

      idx->setup();
      secondary_indices.push_back(idx);
   }

   template <typename PrimaryIndex, typename... SecondaryIndices>
   void init(eosio::name contract, eosio::name table, eosio::name db, PrimaryIndex& prim_index, SecondaryIndices&... indices) {
      validate_types(prim_index);
      (validate_types(indices), ...);

      contract_name = contract;
      table_name = table;
      db_name = db.value;

      primary_index = &prim_index;
      primary_index->contract_name = contract_name;
      primary_index->table_name = table_name;
      primary_index->tbl = this;

      primary_index->setup();

      primary_index_name = primary_index->index_name;

      (setup_indices(indices), ...);
   }

private:

   constexpr void validate_types() {}

   template <typename Type>
   constexpr void validate_types(Type& t) {
      constexpr bool is_kv_index = std::is_base_of_v<kv_index, std::decay_t<Type>>;
      static_assert(is_kv_index, "Incorrect type passed to init. Must be a reference to an index.");
   }

   template <typename V>
   static void serialize(const V& value, void* buffer, size_t size) {
      datastream<char*> ds((char*)buffer, size);
      unsigned_int i{0};
      ds << i;
      ds << value;
   }

   template <typename... Vs>
   static void serialize(const std::variant<Vs...>& value, void* buffer, size_t size) {
      datastream<char*> ds((char*)buffer, size);
      ds << value;
   }

   template <typename V>
   static void deserialize(V& value, void* buffer, size_t size) {
      unsigned_int idx;
      datastream<const char*> ds((char*)buffer, size);

      ds >> idx;
      eosio::check(idx==unsigned_int(0), "there was an error deserializing this value.");
      ds >> value;
   }

   template <typename... Vs>
   static void deserialize(std::variant<Vs...>& value, void* buffer, size_t size) {
      datastream<const char*> ds((char*)buffer, size);
      ds >> value;
   }

   template <typename V>
   static size_t get_size(const V& value) {
      auto size = pack_size(value);
      return size + 1;
   }

   template <typename... Vs>
   static size_t get_size(const std::variant<Vs...>& value) {
      auto size = pack_size(value);
      return size;
   }
};
} // eosio
