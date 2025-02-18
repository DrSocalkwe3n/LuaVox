#pragma once

#include "boost/thread/pthread/condition_variable_fwd.hpp"
#include <libpq-fe.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/endian/buffers.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>

#include <stdexcept>
#include <string>
#include <utility>

#include <boost/thread.hpp>

/*
PSQL::Connection Conn(ioc, "hostaddr= dbname= user= password=");
co_await Conn.async_prepare("name", "query", asio::use_awaitable);

*/

namespace TOS::PSQL {

enum class EnumFieldType : int {
  TEXT = 0,
  BINARY = 1,
};

template <class T, class Enable = void>
class TypeEncoder {
private:
  using no_ref_t = std::remove_const_t<std::remove_reference_t<T>>;

public:
  using encoder_t = std::conditional_t<
    std::is_array_v<no_ref_t>,
    TypeEncoder<std::remove_extent_t<std::add_const_t<no_ref_t>>*>,
    TypeEncoder<no_ref_t>>;
  using value_t = std::string;

public:
  static constexpr std::size_t size(const T& t) {
    return sizeof(t);
  }

  static constexpr int type(const T& t) {
    return 0;
  }

  value_t to_text_value(const T& t) {
    return std::to_string(t);
  }

  const char* c_str(const value_t& t) {
    return t.c_str();
  }
};

template <>
class TypeEncoder<std::string, void> {
public:
  using encoder_t = TypeEncoder<std::string>;
  using value_t = const char*;

public:
  static std::size_t size(const std::string& t) {
    return t.size();
  }

  static constexpr EnumFieldType type(const std::string& t) {
    return EnumFieldType::TEXT;
  }

  value_t to_text_value(const std::string& t) {
    return t.c_str();
  }

  const char* c_str(const std::string& t) {
    return t.c_str();
  }
};

template <>
class TypeEncoder<const char*, void> {
public:
  using encoder_t = TypeEncoder<const char*>;
  using value_t = const char*;

public:
  static std::size_t size(const char* const& t) {
    return std::strlen(t);
  }

  static constexpr EnumFieldType type(const char* const& t) {
    return EnumFieldType::TEXT;
  }

  value_t to_text_value(const char* const& t) {
    return t;
  }

  const char* c_str(const value_t& t) {
    return t;
  }
};

template <class T>
class TypeEncoder<T, std::enable_if_t<std::is_integral_v<T>>> {
public:
  using value_t = T;

public:
  static std::size_t size(const T& t) {
    return sizeof(t);
  }

  static constexpr EnumFieldType type(const T& t) {
    return EnumFieldType::BINARY;
  }

  value_t to_text_value(const T& t) {
    return boost::endian::native_to_big(t);
  }

  const char* c_str(const T& t) {
    return reinterpret_cast<const char*>(&t);
  }
};

template <class T>
class TypeEncoder<T, std::enable_if_t<std::is_floating_point_v<T>>> {
public:
  using value_t = T;

public:
  static std::size_t size(const T& t) {
    return sizeof(t);
  }

  static constexpr EnumFieldType type(const T& t) {
    return EnumFieldType::BINARY;
  }

  value_t to_text_value(const T& t) {
    using namespace boost::endian;

    value_t v;

    endian_store<T, sizeof(T), order::big>(
        reinterpret_cast<unsigned char*>(&v), t);

    return v;
  }

  const char* c_str(const T& t) {
    return reinterpret_cast<const char*>(&t);
  }
};


template <class T, class Enable = void>
class TypeDecoder {
public:
  static constexpr std::size_t min_size = 0;
  static constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
  static constexpr bool nullable = false;

public:
  T from_binary(const char* data, std::size_t length) {
    return data;
  }
};

template <class T>
class TypeDecoder<std::optional<T>, void> {
private:
  using underlying_decoder_t = TypeDecoder<T, void>;

public:
  static constexpr std::size_t min_size = underlying_decoder_t::min_size;
  static constexpr std::size_t max_size = underlying_decoder_t::max_size;

  static constexpr bool nullable = true;

public:
  std::optional<T> from_binary(const char* data, std::size_t length) {
    if (length == 0) {
      return {};
    } else {
      return underlying_decoder_t{}.from_binary(data, length);
    }
  }
};

template <class T>
class TypeDecoder<T, std::enable_if_t<std::is_integral_v<T>>> {
public:
  static constexpr std::size_t min_size = sizeof(T);
  static constexpr std::size_t max_size = sizeof(T);
  static constexpr bool nullable = false;

public:
  T from_binary(const char* data, std::size_t length) {
    using namespace boost::endian;

    return endian_load<T, sizeof(T), order::big>(reinterpret_cast<unsigned const char*>(data));
  }
};

template <class T>
class TypeDecoder<T, std::enable_if_t<std::is_floating_point_v<T>>> {
public:
  static constexpr std::size_t min_size = sizeof(T);
  static constexpr std::size_t max_size = sizeof(T);
  static constexpr bool nullable = false;

public:
  T from_binary(const char* data, std::size_t length) {
    using namespace boost::endian;

    return endian_load<T, sizeof(T), order::big>(reinterpret_cast<unsigned const char*>(data));
  }
};


template <>
class TypeDecoder<const char*, void> {
public:
  static constexpr std::size_t min_size = 0;
  static constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
  static constexpr bool nullable = true;

public:
  const char* from_binary(const char* data, std::size_t length) {
    return data;
  }
};

template <>
class TypeDecoder<std::string, void> {
public:
  static constexpr std::size_t min_size = 0;
  static constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
  static constexpr bool nullable = true;

public:
  std::string from_binary(const char* data, std::size_t length) {
    return std::string(data, length);
  }
};


class Field {
private:
  const PGresult* const Result;
  const size_t IndexRow, IndexColumn;

public:
  Field(const PGresult* res, size_t row, size_t column)
    : Result{res}, IndexRow{row}, IndexColumn{column} {
  }

  template <class T>
  T as() const {
    using decoder_t = TypeDecoder<T>;

    if (!decoder_t::nullable && is_null())
      throw std::length_error{"PSQL: field is null"};

    const int field_length = PQgetlength(Result, IndexRow, IndexColumn);

    if (!(field_length == 0 && decoder_t::nullable) && field_length < decoder_t::min_size || field_length > decoder_t::max_size)
      throw std::length_error("PSQL: Размер поля " + std::to_string(field_length) + " не в пределах " +
        std::to_string(decoder_t::min_size) + "-" + std::to_string(decoder_t::max_size));

    return unsafe_as<T>();
  }

  template <class T>
  T as(T&& default_value) const {
    if (is_null())
      return std::forward<T>(default_value);
    else
      return as<T>();
  }

  template <class T>
  T unsafe_as() const {
    TypeDecoder<T> decoder{};
    return decoder.from_binary(PQgetvalue(Result, IndexRow, IndexColumn), PQgetlength(Result, IndexRow, IndexColumn));
  }

  template <class T>
  T unsafe_as(T&& default_value) const {
    if (is_null())
      return std::forward<T>(default_value);
    else
      return unsafe_as<T>();
  }

  bool is_null() const {
    return PQgetisnull(Result, IndexRow, IndexColumn);
  }

  template<class T>
  void to(T &obj) const {
    obj = as<T>();
  }

  template<class T>
  void to(T &obj, T&& default_value) const {
    obj = as<T>(default_value);
  }
};

namespace utility {

template <class... Params>
std::tuple<typename TypeEncoder<Params>::encoder_t::value_t...>
create_value_holders(Params&&... params) {
  return std::make_tuple(typename TypeEncoder<Params>::encoder_t{}.to_text_value(params)...);
}

template <class... Params>
std::array<const char*, sizeof...(Params)> value_array(Params&&... params) {
  return {typename TypeEncoder<Params>::encoder_t{}.c_str(params)...};
}

template <class... Params>
std::array<int, sizeof...(Params)> size_array(Params&&... params) {
  return {static_cast<int>(typename TypeEncoder<Params>::encoder_t{}.size(params))...};
}

template <class... Params>
std::array<int, sizeof...(Params)> type_array(Params&&... params) {
  return {typename TypeEncoder<Params>::encoder_t{}.type(params)...};
}
}

class Row {
private:
  const PGresult* const Result;
  const size_t IndexRow;

public:
  Row(const PGresult* result, size_t row)
    : Result(result), IndexRow(row)
  {}

  const Field operator[](size_t column) const {
    size_t columns = PQnfields(Result);
    if (column >= columns) 
      throw std::out_of_range{"PSQL: Колонка " + std::to_string(column) + " >= size(" + std::to_string(columns) + ")"};

    return Field(Result, IndexRow, column);
  }

  const Field at(size_t column) const {
    return this->operator[](column);
  }
};


class ResultIterator
  : public std::random_access_iterator_tag {
public:
  //using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

private:
  const PGresult* const Result;
  size_t IndexRow;

public:
  ResultIterator(const PGresult* result)
    : ResultIterator(result, 0) {
  }

  ResultIterator(const PGresult* result, size_t row)
    : Result{result}, IndexRow{row} 
  {}

  const Row operator*() {
    return Row(Result, IndexRow);
  }

  ResultIterator& operator++(int) {
    IndexRow++;
    return *this;
  }

  ResultIterator& operator--(int) {
    IndexRow--;
    return *this;
  }

  ResultIterator operator+(size_t n) const {
    return ResultIterator(Result, IndexRow+n);
  }

  friend inline ResultIterator operator+(size_t n, const ResultIterator& rhs) {
    return ResultIterator(rhs.Result, rhs.IndexRow+n);
  }

  ResultIterator operator-(size_t n) {
    return ResultIterator(Result, IndexRow-n);
  }

  friend inline ResultIterator operator-(size_t n, const ResultIterator& rhs) {
    return ResultIterator(rhs.Result, rhs.IndexRow-n);
  }

  ResultIterator& operator+=(size_t n) {
    IndexRow += n;
    return *this;
  }

  ResultIterator& operator-=(size_t n) {
    IndexRow -= n;
    return *this;
  }

  ResultIterator operator[](size_t n) {
    return ResultIterator(Result, IndexRow+n);
  }

  friend inline difference_type operator-(const ResultIterator& lhs, const ResultIterator& rhs) {
    return lhs.IndexRow - rhs.IndexRow;
  }

  friend inline bool operator==(const ResultIterator& lhs, const ResultIterator& rhs) {
    return lhs.Result == rhs.Result && lhs.IndexRow == rhs.IndexRow;
  }

  friend inline bool operator!=(const ResultIterator& lhs, const ResultIterator& rhs) {
    return lhs.Result != rhs.Result || lhs.IndexRow != rhs.IndexRow;
  }

  friend inline auto operator<=>(const ResultIterator& lhs, const ResultIterator& rhs) {
    return lhs.IndexRow <=> rhs.IndexRow;
  }

  // friend inline bool operator<(const ResultIterator& lhs, const ResultIterator& rhs) {
  //   return lhs.IndexRow < rhs.IndexRow;
  // }

  // friend inline bool operator<=(const ResultIterator& lhs, const ResultIterator& rhs) {
  //   return lhs.IndexRow <= rhs.IndexRow;
  // }

  // friend inline bool operator>(const ResultIterator& lhs, const ResultIterator& rhs) {
  //   return lhs.IndexRow > rhs.IndexRow;
  // }

  // friend inline bool operator>=(const ResultIterator& lhs, const ResultIterator& rhs) {
  //   return lhs.IndexRow >= rhs.IndexRow;
  // }
};

class Result {
private:
  PGresult *Result_;

public:
  using iterator = ResultIterator;
  using ConstIterator = iterator;

  enum class EnumStatus : int {
    EMPTY_QUERY = PGRES_EMPTY_QUERY,
    COMMAND_OK = PGRES_COMMAND_OK,
    TUPLES_OK = PGRES_TUPLES_OK,
    BAD_RESPONSE = PGRES_BAD_RESPONSE,
    FATAL_ERROR = PGRES_FATAL_ERROR,
  };

public:
  Result(PGresult* const& result) noexcept
    : Result_(result) 
  {}

  Result(const Result& other) = delete;

  Result(Result&& other) noexcept
    : Result(other.Result_) 
  {
    other.Result_ = nullptr;
  }

  Result& operator=(const Result& other) = delete;

  Result& operator=(Result&& other) noexcept {
    std::swap(Result_, other.Result_);
    return *this;
  }

  ~Result() {
    if(Result_ != nullptr)
      PQclear(Result_);
  }

  /**
   * If true, indicates that we are done and this result is empty. An empty
   * result is typically used to mark the end of a series of result objects
   * (e.g. \ref Transaction::async_exec_all).
   *
   * The result object is empty when this returns true, therefore,
   * the object must not be used, calling any other member function is invalid.
   */
  bool done() const { return Result_ == nullptr; }

  bool ok() const { return status() == EnumStatus::TUPLES_OK || status() == EnumStatus::COMMAND_OK; }

  EnumStatus status() const { return static_cast<EnumStatus>(PQresultStatus(Result_)); }

  ConstIterator begin() const { return {Result_}; }
  ConstIterator cbegin() const { return begin(); }

  ConstIterator end() const {
    int rows = PQntuples(Result_);
    assert(rows >= 0);
    if (rows < 0) 
      rows = 0;

    return {Result_, size_t(rows)};
  }

  ConstIterator cend() const { return end(); }

  const Row operator[](size_t n) const {
    return *(begin() + n);
  }

  const Row at(size_t n) const {
    if (n >= size()) 
      throw std::out_of_range("PSQL: row " + std::to_string(n) + " >= size(" +  std::to_string(size()) +")");

    return (*this)[n];
  }

  size_t size() const { return PQntuples(Result_); }

  size_t affected_rows() const {
    // const char *s = PQcmdTuples(Result);

    // if (s[0] == '\0')
    //   throw std::runtime_error{"invalid query type for affected rows"};

    return std::stoull(PQcmdTuples(Result_));
  }

  std::string error_message() const { return std::string(PQresultErrorMessage(Result_)); }
};

template <class Derived>
class SocketOperations {
protected:
  SocketOperations() = default;

  SocketOperations(const SocketOperations&) = delete;
  SocketOperations(SocketOperations&&) = default;

  SocketOperations& operator=(const SocketOperations&) = delete;
  SocketOperations& operator=(SocketOperations&&) = default;

  ~SocketOperations() = default;

  template <class ResultCallableT>
  auto handle_exec(ResultCallableT&& handler) {
    auto initiation = [this](ResultCallableT&& handler) {
      auto wrapped_handler = [handler = std::move(handler),
           r = std::make_shared<Result>(nullptr)](ResultCallableT &&res) mutable {
        // if (!res.done()) {
        //   *r = std::move(res);
        // } else {
        //   handler(std::move(*r));
        // }

        if (res.done()) 
          handler(std::move(*r));
      };

      on_write_ready({});
      wait_read_ready(std::move(wrapped_handler));
    };

    return boost::asio::async_initiate<ResultCallableT, void(Result)>(initiation, handler);
  }

  template <class ResultCallableT>
  auto handle_exec_all(ResultCallableT &&handler) {
    auto initiation = [this](ResultCallableT &&handler) {
      on_write_ready({});
      wait_read_ready(std::move(handler));
    };

    return boost::asio::async_initiate<ResultCallableT, void(Result)>(initiation, handler);
  }

private:
  template <class ResultCallableT>
  void wait_read_ready(ResultCallableT &&handler) {
    derived().socket().async_wait(std::decay_t<decltype(derived().socket())>::wait_read,
        [this, handler = std::move(handler)](auto&& ec) mutable {
          on_read_ready(std::move(handler), ec); });
  }

  void wait_write_ready() {
    derived().socket().async_wait(std::decay_t<decltype(derived().socket())>::wait_write,
        std::bind(&SocketOperations::on_write_ready, this, std::placeholders::_1));
  }

  template <class ResultCallableT>
  void on_read_ready(ResultCallableT&& handler, const boost::system::error_code &ec) {
    while (true) {
      if (PQconsumeInput(derived().connection().underlying_handle()) != 1) {
        // TODO: convert this to some kind of error via the callback
        throw std::runtime_error{
          "PSQL: получение не удалось " + std::string{derived().connection().last_error_message()}};
      }

      if (!PQisBusy(derived().connection().underlying_handle())) {
        PGresult *pqres = PQgetResult(derived().connection().underlying_handle());

        handler(Result(pqres));

        if (!pqres) {
          break;
        }
      } else {
        wait_read_ready(std::move(handler));
        break;
      }
    }
  }

  void on_write_ready(const boost::system::error_code &ec) {
    const int ret = PQflush(derived().connection().underlying_handle());
    if (ret == 1) {
      wait_write_ready();
    } else if (ret != 0) {
      // TODO: ignore or convert this to some kind of error via the callback
      throw std::runtime_error{
        "PSQL: отправка не удалась " + std::string{derived().connection().last_error_message()}};
    }
  }

  Derived& derived() { return *static_cast<Derived*>(this); }
};

template <class, class>
class Transaction;

class Connection : public SocketOperations<Connection> {
private:
  boost::asio::ip::tcp::socket Socket;
  PGconn *PGConnection;

  friend class SocketOperations<Connection>;

public:
  template <class Executor>
  Connection(Executor &exc, const std::string &pgconninfo)
    : Socket(exc), PGConnection(PQconnectdb(pgconninfo.c_str()))
  {
    if (status() != CONNECTION_OK)
      throw std::runtime_error{"PSQL: не удалось подключиться " + std::string{PQerrorMessage(PGConnection)}};

    if (PQsetnonblocking(PGConnection, 1) != 0)
      throw std::runtime_error{"PSQL: не удалось установить не блокирующий параметр " + std::string{PQerrorMessage(PGConnection)}};

    const int sock = PQsocket(PGConnection);

    if (sock < 0)
      throw std::runtime_error("PSQL: не удалось получить действительный дескриптор сокета");

    Socket.assign(boost::asio::ip::tcp::v4(), sock);
  }

  ~Connection() {
    if (PGConnection)
      PQfinish(PGConnection);
  }

  Connection(Connection const&) = delete;

  Connection(Connection&& rhs) noexcept
    : Socket(std::move(rhs.Socket)), PGConnection{std::move(rhs.PGConnection)} 
  {
    rhs.PGConnection = nullptr;
  }

  Connection& operator=(Connection const&) = delete;

  Connection& operator=(Connection&& rhs) noexcept {
    std::swap(Socket, rhs.Socket);
    std::swap(PGConnection, rhs.PGConnection);

    return *this;
  }

  template <class CompletionTokenT>
  auto async_prepare(
      const std::string &statement_name,
      const std::string &query,
      CompletionTokenT &&handler) {
    const auto res = PQsendPrepare(connection().underlying_handle(),
        statement_name.c_str(),
        query.c_str(),
        0,
        nullptr);

    if (res != 1) {
      throw std::runtime_error{
        "error preparing statement '" + statement_name + "': " + std::string{connection().last_error_message()}};
    }

    return handle_exec(std::forward<CompletionTokenT>(handler));
  }

  /**
   * Creates a read/write transaction. Make sure the created transaction
   * object lives until you are done with it.
   */
  template <
    class Unused_RWT = void,
    class Unused_IsolationT = void,
    class TransactionHandlerT>
  auto async_transaction(TransactionHandlerT&& handler) {
    using txn_t = Transaction<Unused_RWT, Unused_IsolationT>;

    auto initiation = [this](auto&& handler) {
      auto w = std::make_shared<txn_t>(*this);
      w->async_exec("BEGIN",
          [handler = std::move(handler), w](auto&& res) mutable { handler(std::move(*w)); } );
    };

    return boost::asio::async_initiate<
      TransactionHandlerT, void(txn_t)>(
          initiation, handler);
  }

  PGconn* underlying_handle() { return PGConnection; }

  const PGconn* underlying_handle() const { return PGConnection; }

  boost::asio::ip::tcp::socket& socket() { return Socket; }

  const char* last_error_message() const { return PQerrorMessage(underlying_handle()); }

private:
  int status() const { return PQstatus(PGConnection); }

  Connection& connection() { return *this; }
};

template <class RWT, class IsolationT>
class Transaction : public SocketOperations<Transaction<RWT, IsolationT>> {
  friend class SocketOperations<Transaction<RWT, IsolationT>>;

private:
  Connection *Conn;
  bool Done;

public:
  Transaction(Connection &conn)
    : Conn(&conn), Done(false) 
  {}

  Transaction(const Transaction&) = delete;
  Transaction(Transaction&& rhs) noexcept
    : Conn(rhs.Conn), Done(rhs.Done) 
  {
    rhs.Done = true;
  }

  Transaction& operator=(const Transaction&) = delete;
  Transaction& operator=(Transaction&& rhs) noexcept {
    Conn = rhs.Conn;
    Done = rhs.Done;
    rhs.Done = true;
  }

  /**
   * Destructor.
   * If neither \ref commit() nor \ref rollback() has been used, destructing
   * will do a sync rollback.
   */
  ~Transaction() noexcept(false) {
    if (!Done) {
      const Result res{PQexec(connection().underlying_handle(), "ROLLBACK")};
      if(Result::EnumStatus::COMMAND_OK != res.status())
        throw std::runtime_error("PSQL: Ошибка завершения транзакции: " + res.error_message()); 
    }
  }
  
  /// See \ref async_exec(query, handler, params) for more.
  template <class ResultCallableT>
  auto async_exec(const std::string &query, ResultCallableT&& handler) {
    return async_exec_2(query, std::forward<ResultCallableT>(handler),
        nullptr, nullptr, nullptr, 0);
  }

  /// See \ref async_exec_prepared(statement_name, handler, params) for more.
  template <class ResultCallableT>
  auto async_exec_prepared(const std::string& statement_name,
      ResultCallableT&& handler) {
    return async_exec_prepared_2(statement_name,
        std::forward<ResultCallableT>(handler), nullptr, nullptr, nullptr, 0);
  }

  /**
   * Execute a query asynchronously.
   * \p query must contain a single query. For multiple queries, see
   * \ref async_exec_all(query, handler, params).
   * \p handler will be called once with the result.
   * \p params parameters to pass in the same order to $1, $2, ...
   *
   * This function must not be called again before the handler is called.
   */
  template <class ResultCallableT, class... Params>
  auto async_exec(const std::string &query, ResultCallableT&& handler,
      Params&&... params) {
    using namespace utility;

    const auto value_holders = create_value_holders(params...);
    const auto value_arr = std::apply(
        [this](auto&&... args) { return value_array(args...); },
        value_holders);
    const auto size_arr = size_array(params...);
    const auto type_arr = type_array(params...);

    return async_exec_2(query, std::forward<ResultCallableT>(handler),
        value_arr.data(), size_arr.data(), type_arr.data(), sizeof...(params));
  }

  /**
   * Execute a query asynchronously.
   * \p statement_name prepared statement name.
   * \ref async_exec_all(query, handler, params).
   * \p handler will be called once with the result.
   * \p params parameters to pass in the same order to $1, $2, ...
   *
   * This function must not be called again before the handler is called.
   */
  template <class ResultCallableT, class... Params>
  auto async_exec_prepared(const std::string& statement_name,
      ResultCallableT&& handler, Params&&... params) {
    using namespace utility;

    const auto value_holders = create_value_holders(params...);
    const auto value_arr = std::apply(
        [this](auto&&... args) { return value_array(args...); },
        value_holders);
    const auto size_arr = size_array(params...);
    const auto type_arr = type_array(params...);

    return async_exec_prepared_2(statement_name,
        std::forward<ResultCallableT>(handler), value_arr.data(),
        size_arr.data(),type_arr.data(), sizeof...(params));
  }

  /**
   * Execute queries asynchronously.
   * Supports multiple queries in \p query, separated by ';' but does not
   * support parameter binding.
   * \p handler will be called once for each query and once more with an
   * empty result where \ref result.done() returns true.
   *
   * This function must not be called again before the handler is called
   * with a result where \ref result.done() returns true.
   */
  template <class ResultCallableT>
  auto async_exec_all(const std::string &query, ResultCallableT&& handler) {
    if(!Done)
      throw std::runtime_error("Запрос уже выполняется");

    const auto res = PQsendQuery(connection().underlying_handle(),
        query.c_str());

    if (res != 1) {
      throw std::runtime_error{
        "PSQL: Ошибка выполнения запроса: " + std::string{connection().last_error_message()}};
    }

    return this->handle_exec_all(std::forward<ResultCallableT>(handler));
  }

  template <class ResultCallableT>
  auto commit(ResultCallableT&& handler) {
    const auto initiation = [this](auto&& handler) {
      async_exec("COMMIT", [this, handler = std::move(handler)](auto&& res) mutable {
          Done = true;
          handler(std::forward<decltype(res)>(res));
        });
    };

    return boost::asio::async_initiate<
      ResultCallableT, void(Result)>(
          initiation, handler);
  }

  template <class ResultCallableT>
  auto rollback(ResultCallableT&& handler) {
    const auto initiation = [this](auto&& handler) {
      async_exec("ROLLBACK", [this, handler = std::move(handler)](auto&& res) mutable {
          Done = true;
          handler(std::forward<decltype(res)>(res));
        });
    };

    return boost::asio::async_initiate<
      ResultCallableT, void(Result)>(
          initiation, handler);
  }

protected:
  Connection& connection() { return *Conn; }

private:
  template <class ResultCallableT>
  auto async_exec_2(const std::string &query, ResultCallableT&& handler,
      const char* const* value_arr, const int* size_arr, const int* type_arr,
      std::size_t num_values) {
    if(!Done)
      throw std::runtime_error("PSQL: Запрос уже выполняется");

    const auto res = PQsendQueryParams(connection().underlying_handle(),
        query.c_str(),
        num_values,
        nullptr,
        value_arr,
        size_arr,
        type_arr,
        static_cast<int>(EnumFieldType::BINARY));

    if (res != 1) {
      throw std::runtime_error{
        "PSQL: Ошибка выполнения запроса '" + query + "': " + std::string{connection().last_error_message()}};
    }

    return this->handle_exec(std::forward<ResultCallableT>(handler));
  }

  template <class ResultCallableT>
  auto async_exec_prepared_2(const std::string& statement_name,
      ResultCallableT&& handler, const char* const* value_arr,
      const int* size_arr, const int* type_arr, std::size_t num_values) {
    if(!Done)
      throw std::runtime_error("PSQL: Запрос уже выполняется");

    const auto res = PQsendQueryPrepared(connection().underlying_handle(),
        statement_name.c_str(),
        num_values,
        value_arr,
        size_arr,
        type_arr,
        1);

    if (res != 1) {
      throw std::runtime_error("PSQL: Ошибка выполнения запроса '" + statement_name + "': " + std::string{connection().last_error_message()});
    }

    return this->handle_exec(std::forward<ResultCallableT>(handler));
  }
};

using work = Transaction<void, void>;

/**
 * Asynchronously executes a query.
 * This function must not be called again before the handler is called.
 */
template <class RWT, class IsolationT, class ResultCallableT, class... Params>
inline auto async_exec(Transaction<RWT, IsolationT>& t, const std::string &query,
    ResultCallableT&& handler, Params&&... params) {
  return t.async_exec(query, std::forward<ResultCallableT>(handler), std::forward<Params>(params)...);
}

/**
 * Starts a transaction, asynchronously executes a query and commits the transaction.
 * This function must not be called again before the handler is called.
 */
template <class ResultCallableT, class... Params>
inline auto async_exec(Connection& c, std::string query,
    ResultCallableT&& token, Params... params) 
{
  auto initiation = [](auto&& handler, Connection& c, std::string query, auto&&... params) mutable 
  {
    c.template async_transaction<>([handler = std::move(handler), query = std::move(query), params...](auto txn) mutable 
    {
      std::unique_ptr<work> ptxn = std::make_unique<work>(std::move(txn));
      work &txn_ref = *ptxn;

      auto wrapped_handler = [handler = std::move(handler), ptxn = std::move(ptxn)](auto&& result) mutable 
      {
        if(result.ok()) 
        {
          work &txn_ref = *ptxn;
          txn_ref.commit([ptxn = std::move(ptxn), handler = std::move(handler), result = std::move(result)] (auto&& commit_result) mutable 
          {
            ptxn = nullptr;
            if(commit_result.ok())
              handler(std::exception_ptr(nullptr), std::move(result));
            else
              handler(std::exception_ptr(std::make_exception_ptr(std::runtime_error(commit_result.error_message()))), std::move(commit_result));
          });
        } else {
          ptxn = nullptr;
          handler(std::exception_ptr(std::make_exception_ptr(std::runtime_error(result.error_message()))), std::move(result));
        }
      };

      async_exec(txn_ref, query, std::move(wrapped_handler),
          std::move(params)...);
    });
  };

  return boost::asio::async_initiate<ResultCallableT, void(std::exception_ptr, Result)>(
        initiation, token, std::ref(c), std::move(query), std::forward<decltype(params)>(params)...);
}


/**
 * Asynchronously executes a prepared query.
 * This function must not be called again before the handler is called.
 */
template <class RWT, class IsolationT, class ResultCallableT, class... Params>
inline auto async_exec_prepared(Transaction<RWT, IsolationT>& t, const std::string &name,
    ResultCallableT &&handler, Params&&... params) {
  return t.async_exec_prepared(name, std::forward<ResultCallableT>(handler), std::forward<Params>(params)...);
}

/**
 * Starts a transaction, asynchronously executes a prepared query and commits
 * the transaction.
 * This function must not be called again before the handler is called.
 */
template <class ResultCallableT, class... Params>
inline auto async_exec_prepared(Connection& c, std::string name,
    ResultCallableT&& handler, Params... params) {
  auto initiation = [](auto&& handler, Connection& c, std::string name, auto&&... params) mutable {
   c.template async_transaction<>([
      handler = std::move(handler),
      name = std::move(name),
      params...](auto txn) mutable {
        auto ptxn = std::make_unique<work>(std::move(txn));
        auto& txn_ref = *ptxn;

        auto wrapped_handler = [handler = std::move(handler), ptxn = std::move(ptxn)](auto&& result) mutable {
            if (result.ok()) {
              auto& txn_ref = *ptxn;
              txn_ref.commit([ptxn = std::move(ptxn), handler = std::move(handler), result = std::move(result)]
                            (auto&& commit_result) mutable {
                  if (commit_result.ok()) {
                    handler(std::move(result));
                  } else {
                    handler(std::move(commit_result));
                  }
                });
            } else {
              handler(std::move(result));
            }
          };

        async_exec_prepared(txn_ref, name, std::move(wrapped_handler),
            std::move(params)...);
      });
  };

  return boost::asio::async_initiate<
    ResultCallableT, void(Result)>(
        initiation, handler, std::ref(c), std::move(name),
        std::forward<decltype(params)>(params)...);
}

}
