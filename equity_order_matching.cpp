
#include <iostream>
#include <map>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <array>
#include <string>
#include <cassert>
#include <sstream>

struct order_data_key
{
  uint32_t m_time;
  float m_price;
};

enum class order_side : int {
    sell
  , buy
};

enum class order_type : int {
    market
  , limit
  , limit_ioc
};

char order_type_to_char(order_type ot)
{
  if (ot == order_type::market) {
    return 'M';
  }
  if (ot == order_type::limit) {
    return 'L';
  }
  if (ot == order_type::limit_ioc) {
    return 'I';
  }
  assert(false);
  return 'E';
}

class order_data
{
public:
  order_data(uint64_t id, uint32_t tim, const std::string& nam, float price, uint64_t q, order_side os, order_type ot)
  : m_id(id)
  , m_time(tim)
  , m_symbol(nam)
  , m_price(price)
  , m_q(q)
  , m_order_side(os)
  , m_order_type(ot)
  {
  }

  uint64_t get_id()        const { return m_id; }
  uint32_t get_time()      const { return m_time; }
  std::string get_symbol() const { return m_symbol; }
  float get_price()        const { return m_price; }
  uint64_t get_q()         const { return m_q; }

  void reduce_q(uint64_t q)
  {
    assert(q <= m_q);
    m_q -= q;
    add_matched_q(q);
  }
  void reset_q()
  {
    m_q = 0;
  }
  void set_q(uint64_t q)
  {
    m_q = q;
  }
  order_data_key get_key() const
  {
    return { m_time, m_price };
  }
  void set_price(float p)
  {
    m_price = p;
  }
  order_side get_order_side() const
  {
    return m_order_side;
  }
  order_type get_order_type() const
  {
    return m_order_type;
  }
  void set_time(uint32_t tim)
  {
    m_time = tim;
  }
  uint64_t get_matched_q() const
  {
    return m_matched_q;
  }
private:
  uint64_t m_id;
  uint32_t m_time;
  std::string m_symbol;
  float m_price;
  uint64_t m_q;
  order_side m_order_side;
  order_type m_order_type;
  uint64_t m_matched_q = 0;

  void add_matched_q(uint64_t q)
  {
    m_matched_q += q;
  }
};


template <order_side OrderSide>
class order
{
public:
  order(const order_data& d, bool ioc)
    :m_order_data(d)
    ,m_ioc(ioc)
  {

  }
  const order_data& get_data() const
  {
    return m_order_data;
  }
  void reduce_q(uint64_t q) {
    m_order_data.reduce_q(q);
  }
  uint64_t get_q() const
  {
    return m_order_data.get_q();
  }
  void reset_q()
  {
    m_order_data.reset_q();
  }
  void set_q(uint64_t q)
  {
    m_order_data.set_q(q);
  }
  float get_price() const
  {
    return m_order_data.get_price();
  }
  uint32_t get_time() const
  {
    return m_order_data.get_time();
  }
  order_data_key get_key() const
  {
    return m_order_data.get_key();
  }
  uint64_t get_id() const
  {
    return m_order_data.get_id();
  }
  bool is_ioc() const
  {
    return m_ioc;
  }
  void set_price(float p)
  {
    m_order_data.set_price(p);
  }
  uint64_t get_matched_q() const
  {
    return m_order_data.get_matched_q();
  }
private:
  order_data m_order_data;
  bool m_ioc = false;
};

class order_sell : public order<order_side::sell>
{
public:
  order_sell(const order_data& od, bool ioc = false)
    :order(od, ioc)
  {

  }
  
};

using limit_order_sell = order_sell;

class order_buy : public order<order_side::buy>
{
public:
  order_buy(const order_data& od, bool ioc = false)
    :order(od, ioc)
  {

  }
};

using limit_order_buy = order_buy;


struct limit_order_buy_less
{
  bool operator()(const order_data_key& k1, const order_data_key& k2) const
  {
    if (k1.m_price < k2.m_price) {
      return false;
    }
    if (k1.m_price > k2.m_price) {
      return true;
    }
    return k1.m_time < k2.m_time;
  }
};

struct limit_order_sell_less
{
  bool operator()(const order_data_key& k1, const order_data_key& k2) const
  {
    if (k1.m_price < k2.m_price) {
      return true;
    }
    if (k1.m_price > k2.m_price) {
      return false;
    }

    return k1.m_time < k2.m_time;
  }
};

struct match_result
{
  uint64_t m_count_buy = 0;
  uint64_t m_count_sell = 0;

  bool no_match() const
  {
    return m_count_buy == 0 && m_count_sell == 0;
  }

  uint64_t get_matched_count() const
  {
    if (m_count_buy) {
      return m_count_buy;
    }
    return m_count_sell;
  }
};

match_result make_buy_match_result(uint64_t matched_buy_count)
{
  return {matched_buy_count, 0};
}

match_result make_sell_match_result(uint64_t matched_sell_count)
{
  return { 0, matched_sell_count };
}


match_result match_limit_orders(limit_order_buy& buy, limit_order_sell& sell)
{
  if (buy.get_data().get_price() < sell.get_data().get_price()) {
    return {};
  }
  
  auto sell_q = sell.get_data().get_q();
  auto buy_q = buy.get_data().get_q();

  if (buy_q > sell_q) {
    buy.reduce_q(sell_q);
    sell.reset_q();
    return make_sell_match_result(sell_q);
  }
  else if (buy_q < sell_q) {
    sell.reduce_q(buy_q);
    buy.reset_q();
    return make_buy_match_result(buy_q);
  }
  else {
    buy.reset_q();
    sell.reset_q();

    return {sell_q, buy_q};
  }
}

void basic_limit_orders_matching_tests()
{
  {
    limit_order_buy order_buy({1, 1, "a", 1.0f, 1, order_side::buy, order_type::limit});
    limit_order_sell order_sell({1, 1, "a", 1.0f, 1, order_side::sell, order_type::limit});

    auto res = match_limit_orders(order_buy, order_sell);

    assert(res.m_count_buy == 1);
    assert(res.m_count_sell == 1);
  }
  {
    limit_order_buy order_buy({1, 1, "a", 2.0f, 1, order_side::buy, order_type::limit});
    limit_order_sell order_sell({1, 1, "a", 1.0f, 1, order_side::sell, order_type::limit});

    auto res = match_limit_orders(order_buy, order_sell);

    assert(res.m_count_buy == 1);
    assert(res.m_count_sell == 1);
  }
  {
    limit_order_buy order_buy({1, 1, "a", 1.0f, 1, order_side::buy, order_type::limit});
    limit_order_sell order_sell({1, 1, "a", 2.0f, 1, order_side::sell, order_type::limit});

    auto res = match_limit_orders(order_buy, order_sell);

    assert(res.m_count_buy == 0);
    assert(res.m_count_sell == 0);
  }
  {
    limit_order_buy order_buy({1, 1, "a", 2.0f, 2, order_side::buy, order_type::limit});
    limit_order_sell order_sell({1, 1, "a", 1.0f, 1, order_side::sell, order_type::limit});

    auto res = match_limit_orders(order_buy, order_sell);

    assert(res.m_count_buy == 0);
    assert(res.m_count_sell == 1);

    assert(order_buy.get_q() == 1);
    assert(order_sell.get_q() == 0);
  }
  {
    limit_order_buy order_buy({1, 1, "a", 2.0f, 1, order_side::buy, order_type::limit});
    limit_order_sell order_sell({1, 1, "a", 1.0f, 2, order_side::sell, order_type::limit});

    auto res = match_limit_orders(order_buy, order_sell);

    assert(res.m_count_buy == 1);
    assert(res.m_count_sell == 0);

    assert(order_buy.get_q() == 0);
    assert(order_sell.get_q() == 1);
  }
}


template <class Ord, class Cmp>
class limit_order_queue : public std::multimap<order_data_key, Ord, Cmp>
{
public:
  bool put_order(const Ord& o)
  {
    if (m_ids.count(o.get_id()) != 0) {
      return false;
    }
    auto iter = this->insert(std::make_pair(o.get_key(), o));
    m_ids.insert(std::make_pair(o.get_id(), iter));
    if (o.is_ioc()) {
      m_ioc.insert(o.get_id());
    }
    return true;
  }
  bool cancel_order(uint64_t id, bool check_ioc = true)
  {
    auto elem = m_ids.find(id);
    if (elem == m_ids.end()) {
      return false;
    }
    if (check_ioc) {
      if (elem->second->second.is_ioc()) {
        m_ioc.erase(id);
      }
    }
    this->erase(elem->second);
    m_ids.erase(elem);
    return true;
  }
  void drop_ioc()
  {
    for (auto id : m_ioc) {
      cancel_order(id, false);
    }
    m_ioc.clear();
  }
  bool id_exists(uint64_t id) const
  {
    auto cnt = m_ids.count(id);
    if (cnt > 0) {
      return true;
    }
    return false;
  }
  float get_best_price() const
  {
    assert(!this->empty());
    auto iter = this->begin();
    return iter->second.get_price();
  }
  Ord& top_order()
  {
    assert(!this->empty());
    auto iter = this->begin();
    return iter->second;
  }
  void pop_order()
  {
    assert(!this->empty());
    auto iter = this->begin();
    m_ids.erase(iter->second.get_id());
    this->erase(iter);
  }

  using queue_iter = typename std::multimap<order_data_key, Ord, Cmp>::iterator;
  queue_iter find_order_iter(uint64_t ord_id)
  {
    auto iter = m_ids.find(ord_id);
    if (iter == m_ids.end()) {
      return this->end();
    }
    return iter->second;
  }
private:
  std::unordered_map<uint64_t, queue_iter> m_ids;
  std::unordered_set<uint64_t> m_ioc;
};

class limit_order_buy_queue: public limit_order_queue<limit_order_buy, limit_order_buy_less>
{
public:
};

class limit_order_sell_queue: public limit_order_queue<limit_order_sell, limit_order_sell_less>
{
public:
};


//template <class Ord>
//class market_order_cont : public std::unordered_map<uint64_t, Ord>
//{
//public:
//  bool id_exists(uint64_t id) const
//  {
//    auto cnt = this->count(id);
//    if (cnt > 0) {
//      return true;
//    }
//    return false;
//  }
//
//  bool put_order(const Ord& o)
//  {
//    auto ret = this->insert(std::make_pair(o.get_id(), o));
//    return ret.second;
//  }
//  bool cancel_order(uint64_t id)
//  {
//    auto iter = this->find(id);
//    if (iter == this->end()) {
//      return false;
//    }
//    this->erase(iter);
//    return true;
//  }
//};

using market_order_buy_cont = limit_order_queue<limit_order_buy, limit_order_buy_less>;
using market_order_sell_cont = limit_order_queue<limit_order_sell, limit_order_sell_less>;

void basic_buy_queue_tests()
{
  {
    limit_order_buy_queue bq;
    limit_order_buy b1({ 1, 1, "n", 1.0f, 1, order_side::buy, order_type::limit});
    bq.put_order(b1);

    limit_order_buy b2({ 2, 1, "n", 1.0f, 1, order_side::buy, order_type::limit});
    bq.put_order(b2);

    assert(bq.size() == 2);
    {
      auto iter = bq.begin();
      assert(iter->second.get_id() == 1);
      ++iter;
      assert(iter->second.get_id() == 2);
    }
  }
  {
    limit_order_buy_queue bq;
    limit_order_buy b1({ 1, 2, "n", 1.0f, 1, order_side::buy, order_type::limit });
    bq.put_order(b1);

    limit_order_buy b2({ 2, 1, "n", 1.0f, 1, order_side::buy, order_type::limit });
    bq.put_order(b2);

    assert(bq.size() == 2);
    {
      auto iter = bq.begin();
      assert(iter->second.get_id() == 2);
      ++iter;
      assert(iter->second.get_id() == 1);
    }
  }

  {
    limit_order_buy_queue bq;
    limit_order_buy b1({ 1, 1, "n", 2.0f, 1, order_side::buy, order_type::limit });
    bq.put_order(b1);

    limit_order_buy b2({ 2, 1, "n", 1.0f, 1, order_side::buy, order_type::limit });
    bq.put_order(b2);

    assert(bq.size() == 2);
    {
      auto iter = bq.begin();
      assert(iter->second.get_id() == 1);
      ++iter;
      assert(iter->second.get_id() == 2);
    }
  }
}

void basic_sell_queue_tests()
{
  {
    limit_order_sell_queue bq;
    limit_order_sell b1({ 1, 1, "n", 1.0f, 1, order_side::sell, order_type::limit });
    bq.put_order(b1);

    limit_order_sell b2({ 2, 1, "n", 1.0f, 1, order_side::sell, order_type::limit });
    bq.put_order(b2);

    assert(bq.size() == 2);
    {
      auto iter = bq.begin();
      assert(iter->second.get_id() == 1);
      ++iter;
      assert(iter->second.get_id() == 2);
    }
  }
  {
    limit_order_sell_queue bq;
    limit_order_sell b1({ 1, 2, "n", 1.0f, 1, order_side::sell, order_type::limit });
    bq.put_order(b1);

    limit_order_sell b2({ 2, 1, "n", 1.0f, 1, order_side::sell, order_type::limit });
    bq.put_order(b2);

    assert(bq.size() == 2);
    {
      auto iter = bq.begin();
      assert(iter->second.get_id() == 2);
      ++iter;
      assert(iter->second.get_id() == 1);
    }
  }
  {
    limit_order_sell_queue bq;
    limit_order_sell b1({ 1, 1, "n", 2.0f, 1, order_side::sell, order_type::limit });
    bq.put_order(b1);

    limit_order_sell b2({ 2, 1, "n", 1.0f, 1, order_side::sell, order_type::limit });
    bq.put_order(b2);

    assert(bq.size() == 2);
    {
      auto iter = bq.begin();
      assert(iter->second.get_id() == 2);
      ++iter;
      assert(iter->second.get_id() == 1);
    }
  }
}

template <typename E>
constexpr auto to_underlying(E e) noexcept
{
  return static_cast<std::underlying_type_t<E>>(e);
}

std::string float_to_string(float val)
{
  std::ostringstream str;
  str.precision(2);
  str.setf(std::ios::fixed, std::ios::floatfield);
  str << val;
  return str.str();
}

struct matched_result_detail
{
  std::string m_symb;
  struct matched_buy {
    uint64_t m_order_id;
    order_type m_order_type;
    uint64_t m_q;
    float m_price;

    std::string to_string() const
    {
      std::string ret;

      ret += std::to_string(m_order_id);
      ret += ',';
      ret += order_type_to_char(m_order_type);
      ret += ',';
      ret += std::to_string(m_q);
      ret += ',';
      ret += float_to_string(m_price);

      return ret;
    }
  };
  struct matched_sell {
    float m_price;
    uint64_t m_q;
    order_type m_order_type;
    uint64_t m_id;

    std::string to_string() const
    {
      std::string ret;

      ret += float_to_string(m_price);
      ret += ',';
      ret += std::to_string(m_q);
      ret += ',';
      ret += order_type_to_char(m_order_type);
      ret += ',';
      ret += std::to_string(m_id);
      return ret;
    }
  };

  matched_buy m_matched_buy;
  matched_sell m_matched_sell;

  std::string to_string() const
  {
    std::string ret;

    ret += m_symb;
    ret += '|';
    ret += m_matched_buy.to_string();
    ret += '|';
    ret += m_matched_sell.to_string();

    return ret;
  }
};

class order_engine
{
public:
  order_engine()
  {
    init_put_func();
  }

  bool put_order(const order_data& d, order_side os, order_type ot)
  {
    if (id_exists(d.get_id())) {
      return false;
    }

    auto ret = m_put_func.at(to_underlying(os)).at(to_underlying(ot))(d);
 
    return ret;
  }

  bool amend_order(const order_data& od)
  {
    {
      auto res = try_amend_order_in_limit_queue(m_limit_buy_queue, od);
      switch (res) {
      case amend_result::failed:
        return false;
      case amend_result::ok:
        return true;
      case amend_result::executed:
        return true;
      };
    }
    {
      auto res = try_amend_order_in_limit_queue(m_limit_sell_queue, od);
      switch (res) {
      case amend_result::failed:
        return false;
      case amend_result::ok:
        return true;
      case amend_result::executed:
        return true;
      };
    }
    {
      auto res = try_amend_order_in_market_queue(m_market_order_buy_cont, od);
      switch (res) {
      case amend_result::failed:
        return false;
      case amend_result::ok:
        return true;
      case amend_result::executed:
        return true;
      };
    }
    {
      auto res = try_amend_order_in_market_queue(m_market_order_sell_cont, od);
      switch (res) {
      case amend_result::failed:
        return false;
      case amend_result::ok:
        return true;
      case amend_result::executed:
        return true;
      };
    }
    return false;
  }

  bool cancel_order(uint64_t id)
  {
    if (m_limit_buy_queue.cancel_order(id)) {
      return true;
    }
    if (m_limit_sell_queue.cancel_order(id)) {
      return true;
    }
    if (m_market_order_buy_cont.cancel_order(id)) {
      return true;
    }
    if (m_market_order_sell_cont.cancel_order(id)) {
      return true;
    }
    return false;
  }

  std::vector<matched_result_detail> run_matching()
  {
    std::vector<matched_result_detail> ret;
    {
      // first check market orders
      std::vector<market_order_buy_cont::iterator> market_buy_orders_to_remove;
      for (auto buy_iter = m_market_order_buy_cont.begin(); buy_iter != m_market_order_buy_cont.end(); ) {
        if (m_limit_sell_queue.empty()) {
          break;
        }
        auto best_price = m_limit_sell_queue.get_best_price();
        limit_order_buy& market_buy_order = buy_iter->second;
        market_buy_order.set_price(best_price);
        auto& sell_order = m_limit_sell_queue.top_order();
        auto match_result = match_limit_orders(market_buy_order, sell_order);

        auto matched_count = match_result.get_matched_count();
        ret.push_back(fill_details(sell_order, market_buy_order, matched_count));

        if (match_result.m_count_buy) {
          // buy market buy order is fully matched, move to the next market buy order
          market_buy_orders_to_remove.push_back(buy_iter);
          ++buy_iter;
        }
        if (match_result.m_count_sell) {
          //limit sell order is fully matched, remove it from the queue
          m_limit_sell_queue.pop_order();
        }
      }
      for (auto iter : market_buy_orders_to_remove) {
        m_market_order_buy_cont.erase(iter);
      }
    }
    {
      // check market sell orders
      std::vector<market_order_sell_cont::iterator> market_sell_orders_to_remove;

      for (auto sell_iter = m_market_order_sell_cont.begin(); sell_iter != m_market_order_sell_cont.end(); ) {
        if (m_limit_buy_queue.empty()) {
          break;
        }

        auto best_price = m_limit_buy_queue.get_best_price();
        limit_order_sell& market_sell_order = sell_iter->second;
        market_sell_order.set_price(best_price);

        auto& buy_order = m_limit_buy_queue.top_order();

        auto match_result = match_limit_orders(buy_order, market_sell_order);

        auto matched_count = match_result.get_matched_count();
        ret.push_back(fill_details(market_sell_order, buy_order, matched_count));

        if (match_result.m_count_buy) {
          // buy market buy order is fully matched, move to the next market buy order

          m_limit_buy_queue.pop_order();
        }
        if (match_result.m_count_sell) {
          //limit sell order is fully matched, remove it from the queue
          market_sell_orders_to_remove.push_back(sell_iter);
          ++sell_iter;
        }
      }
      for (auto iter : market_sell_orders_to_remove) {
        m_market_order_sell_cont.erase(iter);
      }
    }
    {
      // now match limit orders
      for (;;) {
        if (m_limit_buy_queue.empty()) {
          break;
        }
        if (m_limit_sell_queue.empty()) {
          break;
        }
        auto& buy_order = m_limit_buy_queue.top_order();
        auto& sell_order = m_limit_sell_queue.top_order();
        auto match_result = match_limit_orders(buy_order, sell_order);
        if (match_result.no_match()) {
          break;
        }
        auto matched_count = match_result.get_matched_count();
        ret.push_back(fill_details(sell_order, buy_order, matched_count));
        if (match_result.m_count_buy) {
          // buy order fully matched
          m_limit_buy_queue.pop_order();
        }
        if (match_result.m_count_sell) {
          // sell order fully matched
          m_limit_sell_queue.pop_order();
        }
      }
    }
    {
      // cancel all ioc orders
      m_limit_buy_queue.drop_ioc();
      m_limit_sell_queue.drop_ioc();
    }
    return ret;
  }
  void dump_data(uint64_t time)
  {
    //if (time == 0) {
      auto limit_buy_queue_iter = m_limit_buy_queue.begin();
      auto limit_sell_queue_iter = m_limit_sell_queue.begin();
      for (size_t i = 0; i < 5; ++i) {
        std::string symb;
        std::string buy_state;
        if (limit_buy_queue_iter != m_limit_buy_queue.end()) {
          symb = limit_buy_queue_iter->second.get_data().get_symbol();

          buy_state += std::to_string(limit_buy_queue_iter->second.get_id());
          buy_state += ',';
          buy_state += order_type_to_char(limit_buy_queue_iter->second.get_data().get_order_type());
          buy_state += ',';
          buy_state += std::to_string(limit_buy_queue_iter->second.get_q());
          buy_state += ',';
          buy_state += float_to_string(limit_buy_queue_iter->second.get_price());

          ++limit_buy_queue_iter;
        }

        std::string sell_state;
        if (limit_sell_queue_iter != m_limit_sell_queue.end()) {
          symb = limit_sell_queue_iter->second.get_data().get_symbol();

          sell_state += float_to_string(limit_sell_queue_iter->second.get_price());
          sell_state += ',';
          sell_state += std::to_string(limit_sell_queue_iter->second.get_q());
          sell_state += ',';
          sell_state += order_type_to_char(limit_sell_queue_iter->second.get_data().get_order_type());
          sell_state += ',';
          sell_state += std::to_string(limit_sell_queue_iter->second.get_id());

          ++limit_sell_queue_iter;
        }
        if (!symb.empty()) {
          std::cout << symb << '|' << buy_state << '|' << sell_state << '\n';
        }
        else {
          break;
        }
      }
    //}
    //else {
    //  // search for a specific time

    //  auto limit_buy_queue_iter = m_limit_buy_queue.begin();
    //  auto limit_sell_queue_iter = m_limit_sell_queue.begin();
    //  for (;;) {
    //    std::string symb;
    //    std::string buy_state;
    //    if (limit_buy_queue_iter != m_limit_buy_queue.end()) {
    //      if (limit_buy_queue_iter->second.get_time() <= time) {
    //        symb = limit_buy_queue_iter->second.get_data().get_symbol();

    //        buy_state += std::to_string(limit_buy_queue_iter->second.get_id());
    //        buy_state += ',';
    //        buy_state += order_type_to_char(limit_buy_queue_iter->second.get_data().get_order_type());
    //        buy_state += ',';
    //        buy_state += std::to_string(limit_buy_queue_iter->second.get_q());
    //        buy_state += ',';
    //        buy_state += float_to_string(limit_buy_queue_iter->second.get_price());
    //      }

    //      ++limit_buy_queue_iter;
    //    }

    //    std::string sell_state;
    //    if (limit_sell_queue_iter != m_limit_sell_queue.end()) {
    //      if (limit_sell_queue_iter->second.get_time() <= time) {
    //        symb = limit_sell_queue_iter->second.get_data().get_symbol();

    //        sell_state += float_to_string(limit_sell_queue_iter->second.get_price());
    //        sell_state += ',';
    //        sell_state += std::to_string(limit_sell_queue_iter->second.get_q());
    //        sell_state += ',';
    //        sell_state += order_type_to_char(limit_sell_queue_iter->second.get_data().get_order_type());
    //        sell_state += ',';
    //        sell_state += std::to_string(limit_sell_queue_iter->second.get_id());
    //      }
    //      ++limit_sell_queue_iter;
    //    }
    //    if (!symb.empty()) {
    //      std::cout << symb << '|' << buy_state << '|' << sell_state << '\n';
    //    }
    //    if (   limit_buy_queue_iter == m_limit_buy_queue.end() 
    //        || limit_sell_queue_iter == m_limit_sell_queue.end()) {
    //      break;
    //    }
    //  }
    //}
  }
private:
  matched_result_detail fill_details(const order_sell& sell_order, const order_buy& buy_order, uint64_t matched_count) const
  {
    matched_result_detail det;

    det.m_symb = sell_order.get_data().get_symbol();

    det.m_matched_buy.m_order_id = buy_order.get_id();
    det.m_matched_buy.m_order_type = buy_order.get_data().get_order_type();
    det.m_matched_buy.m_price = sell_order.get_price();//buy_order.get_price();
    det.m_matched_buy.m_q = matched_count;

    det.m_matched_sell.m_id = sell_order.get_id();
    det.m_matched_sell.m_order_type = sell_order.get_data().get_order_type();
    det.m_matched_sell.m_price = sell_order.get_price();
    det.m_matched_sell.m_q = matched_count;

    return det;
  }
  enum class amend_result {
    not_found
    , failed
    , ok
    , executed
  };

  template <class Q>
  amend_result try_amend_order_in_market_queue(Q& q, const order_data& new_order_data) const
  {
    auto iter = q.find_order_iter(new_order_data.get_id());
    if (iter == q.end()) {
      return amend_result::not_found;
    }
    auto amend_typ = check_amend_type(iter->second.get_data(), new_order_data);
    switch (amend_typ) {
    case amend_type::invalid:
      return amend_result::failed;
    case amend_type::q:
      iter->second.set_q(new_order_data.get_q());
      break;
    case amend_type::price:
      iter->second.set_price(new_order_data.get_price());
      break;
    case amend_type::price_and_q:
      iter->second.set_q(new_order_data.get_q());
      iter->second.set_price(new_order_data.get_price());
      break;
    }
    return amend_result::ok;
  }

  template <class Q>
  amend_result try_amend_order_in_limit_queue(Q& q, const order_data& new_order_data) const
  {
    auto iter = q.find_order_iter(new_order_data.get_id());
    if (iter == q.end()) {
      return amend_result::not_found;
    }
    auto amend_typ = check_amend_type(iter->second.get_data(), new_order_data);
    order_data cur_data = iter->second.get_data();

    switch (amend_typ) {
    case amend_type::invalid:
      return amend_result::failed;
    case amend_type::q:
    {
      if (new_order_data.get_q() <= cur_data.get_matched_q()) {
        q.cancel_order(cur_data.get_id());
        return amend_result::executed;
      }
      auto old_q = cur_data.get_q();
      cur_data.set_q(new_order_data.get_q() - cur_data.get_matched_q());
      if (cur_data.get_q() > old_q) {
        cur_data.set_time(new_order_data.get_time());
      }
      break;
    }
    case amend_type::price:
      cur_data.set_price(new_order_data.get_price());
      cur_data.set_time(new_order_data.get_time());
      break;
    case amend_type::price_and_q:
      if (new_order_data.get_q() <= cur_data.get_matched_q()) {
        q.cancel_order(cur_data.get_id());
        return amend_result::executed;
      }
      cur_data.set_q(new_order_data.get_q() - cur_data.get_matched_q());
      cur_data.set_price(new_order_data.get_price());
      cur_data.set_time(new_order_data.get_time());
      break;
    };
    //cur_data.set_time(new_order_data.get_time());
    // remove the order
    q.cancel_order(cur_data.get_id());
    // put it again
    q.put_order(cur_data);
    return amend_result::ok;
  }

  enum class amend_type {
      invalid
    , q
    , price
    , price_and_q
  };
  amend_type check_amend_type(const order_data& old_data, const order_data& new_data) const
  {
    if (old_data.get_id() != new_data.get_id()) {
      return amend_type::invalid;
    }
    if (old_data.get_order_side() != new_data.get_order_side()) {
      return amend_type::invalid;
    }
    if (old_data.get_order_type() != new_data.get_order_type()) {
      return amend_type::invalid;
    }
    if (old_data.get_price() != new_data.get_price()) {
      if (old_data.get_q() != new_data.get_q()) {
        return amend_type::price_and_q;
      }
      return amend_type::price;
    }
    if (old_data.get_q() != new_data.get_q()) {
      return amend_type::q;
    }
    return amend_type::invalid;
  }
  bool id_exists(uint64_t id) const
  {
    // check all queues for the id
    if (m_limit_buy_queue.id_exists(id)) {
      return true;
    }
    if (m_limit_sell_queue.id_exists(id)) {
      return true;
    }

    if (m_market_order_buy_cont.id_exists(id)) {
      return true;
    }
    if (m_market_order_sell_cont.id_exists(id)) {
      return true;
    }
    return false;
  }
  
  bool put_order_sell_market(const order_data& d)
  {
    auto ret = m_market_order_sell_cont.put_order(d);
    return ret;
  }
  bool put_order_sell_limit(const order_data& d)
  {
    auto ret = m_limit_sell_queue.put_order(limit_order_sell(d));
    return ret;
  }
  bool put_order_sell_limit_ioc(const order_data& d)
  {
    auto ret = m_limit_sell_queue.put_order(limit_order_sell(d, true));
    return ret;
  }
  bool put_order_buy_market(const order_data& d)
  {
    auto ret = m_market_order_buy_cont.put_order(d);
    return ret;
  }
  bool put_order_buy_limit(const order_data& d)
  {
    auto ret = m_limit_buy_queue.put_order(limit_order_buy(d));
    return ret;
  }
  bool put_order_buy_limit_ioc(const order_data& d)
  {
    auto ret = m_limit_buy_queue.put_order(limit_order_buy(d, true));
    return ret;
  }
  void init_put_func()
  {
    m_put_func.at(to_underlying(order_side::buy)).at(to_underlying(order_type::limit)) = [this](const auto& od) {return put_order_buy_limit(od); };
    m_put_func.at(to_underlying(order_side::buy)).at(to_underlying(order_type::market)) = [this](const auto& od) {return put_order_buy_market(od); };
    m_put_func.at(to_underlying(order_side::buy)).at(to_underlying(order_type::limit_ioc)) = [this](const auto& od) {return put_order_buy_limit_ioc(od); };

    m_put_func.at(to_underlying(order_side::sell)).at(to_underlying(order_type::limit)) = [this](const auto& od) {return put_order_sell_limit(od); };
    m_put_func.at(to_underlying(order_side::sell)).at(to_underlying(order_type::market)) = [this](const auto& od) {return put_order_sell_market(od); };
    m_put_func.at(to_underlying(order_side::sell)).at(to_underlying(order_type::limit_ioc)) = [this](const auto& od) {return put_order_sell_limit_ioc(od); };
  }
  limit_order_buy_queue m_limit_buy_queue;
  limit_order_sell_queue m_limit_sell_queue;

  market_order_buy_cont m_market_order_buy_cont;
  market_order_sell_cont m_market_order_sell_cont;

  std::array<std::array<std::function<bool(const order_data&)>, 3>, 2> m_put_func;
};

class parse_error
{
public:
  virtual std::string get_msg() const = 0;
};

class dummy_parse_error : public parse_error
{
public:
  std::string get_msg() const override
  {
    assert(false);
    return std::string("");
  }
};

class new_parse_error : public parse_error
{
public:
  new_parse_error(uint64_t order_id)
    :m_order_id(order_id)
  {

  }
  std::string get_msg() const override
  {
    std::string msg;
    msg += std::to_string(m_order_id);
    msg += " - Reject - 303 - Invalid order details";
    return msg;
  }
private:
  uint64_t m_order_id;
};

class amend_not_found_error : public parse_error
{
public:
  amend_not_found_error(uint64_t order_id)
    :m_order_id(order_id)
  {

  }
  std::string get_msg() const override
  {
    std::string msg;
    msg += std::to_string(m_order_id);
    msg += " - AmendReject - 404 - Order does not exist";
    return msg;
  }
private:
  uint64_t m_order_id;
};

class amend_parse_error : public parse_error
{
public:
  amend_parse_error(uint64_t order_id)
    :m_order_id(order_id)
  {

  }
  std::string get_msg() const override
  {
    std::string msg;
    msg += std::to_string(m_order_id);
    msg += " - AmendReject - 404 - Invalid amendment details";
    return msg;
  }
private:
  uint64_t m_order_id;
};

class cancel_not_found_error : public parse_error
{
public:
  cancel_not_found_error(uint64_t order_id)
    :m_order_id(order_id)
  {

  }
  std::string get_msg() const override
  {
    std::string msg;
    msg += std::to_string(m_order_id);
    msg += " - CancelReject - 404 - Order does not exist";
    return msg;
  }
private:
  uint64_t m_order_id;
};

class query_data
{
public:
  query_data()
  {

  }
  query_data(const std::string& symb)
    :m_symbol(symb)
  {

  }
  query_data(uint32_t time)
    :m_timestamp(time)
  {

  }
  query_data(const std::string& symb, uint32_t time)
    :m_symbol(symb)
    , m_timestamp(time)
  {

  }
  std::string get_symb() const
  {
    return m_symbol;
  }
  uint32_t get_timestamp() const
  {
    return m_timestamp;
  }
private:
  std::string m_symbol;
  uint32_t    m_timestamp = 0;
};

class order_engines //: public 
{
public:
  void add_order_from_order_data(const order_data& od)
  {
    if (od.get_time() < m_current_time) {
      throw new_parse_error(od.get_id());
    }
    m_current_time = od.get_time();
    auto res = m_engines[od.get_symbol()].put_order(od, od.get_order_side(), od.get_order_type());
    if (res == false) {
      throw new_parse_error(od.get_id());
    }

    m_symbols[od.get_id()] = od.get_symbol();
    std::cout << od.get_id() << " - Accept\n";
  }
  void amend_order_from_order_data(const order_data& od)
  {
    if (od.get_time() < m_current_time) {
      throw amend_parse_error(od.get_id());
    }
    m_current_time = od.get_time();

    auto symbol_iter = m_symbols.find(od.get_id());
    if (symbol_iter == m_symbols.end()) {
      throw amend_not_found_error(od.get_id());
    }
    if (symbol_iter->second != od.get_symbol()) {
      throw amend_parse_error(od.get_id());
    }
    auto engine_iter = m_engines.find(od.get_symbol());
    assert(engine_iter != m_engines.end());
    bool amend_res = engine_iter->second.amend_order(od);
    if (!amend_res) {
      throw amend_not_found_error(od.get_id());
    }
    std::cout << od.get_id() << " - AmendAccept\n";
  }
  void cancel_order(uint64_t id)
  {
    auto symb_iter = m_symbols.find(id);
    if (symb_iter == m_symbols.end()) {
      throw cancel_not_found_error(id);
    }
    auto eng_iter = m_engines.find(symb_iter->second);
    assert(eng_iter != m_engines.end());
    auto res = eng_iter->second.cancel_order(id);
    if (!res) {
      throw cancel_not_found_error(id);
    }
    std::cout << id << " - CancelAccept\n";
  }
  std::vector<matched_result_detail> match_one(const std::string& symb)
  {
    auto iter = m_engines.find(symb);
    assert(iter != m_engines.end());
    auto ret = iter->second.run_matching();
    return ret;
  }
  std::vector<matched_result_detail> match_all()
  {
    std::vector<matched_result_detail> ret;
    for (auto& eng : m_engines) {
      auto cur = eng.second.run_matching();
      ret.insert(ret.end(), cur.begin(), cur.end());
    }
    return ret;
  }
  void dump_data(const query_data& q)
  {
    auto symb = q.get_symb();
    auto time = q.get_timestamp();
    if (symb.empty()) {
      for (auto& eng : m_engines) {
        eng.second.dump_data(time);
      }
    }
    else {
      auto iter = m_engines.find(symb);
      assert(iter != m_engines.end());
      iter->second.dump_data(time);
    }
  }
private:
  uint32_t m_current_time = 0;
  std::map<std::string, order_engine> m_engines;
  std::unordered_map<uint64_t, std::string> m_symbols;
};


std::vector<std::string> split_string(const std::string& line)
{
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(line);
  while (std::getline(tokenStream, token, ','))
  {
    tokens.push_back(token);
  }
  return tokens;
}

order_type string_to_order_type(const std::string& s)
{
  if (s.length() != 1) {
    throw dummy_parse_error();
  }
  switch (s[0]) {
  case 'M' :
    return order_type::market;
  case 'L':
    return order_type::limit;
  case 'I':
    return order_type::limit_ioc;
  }
  throw dummy_parse_error();
}

order_side string_to_order_side(const std::string& s)
{
  if (s.length() != 1) {
    throw dummy_parse_error();
  }

  switch (s[0]) {
  case 'S':
    return order_side::sell;
  case 'B':
    return order_side::buy;
  }

  throw dummy_parse_error();
}

order_data parse_new_order_string(const std::string& line)
{
  auto tok = split_string(line);
  assert(tok.size() == 8);

  uint64_t order_id = std::stoull(tok[1]);
  try {
    uint32_t time_stamp = std::stoul(tok[2]);
    std::string symb = tok[3];
    order_type ord_type = string_to_order_type(tok[4]);
    order_side ord_side = string_to_order_side(tok[5]);
    float price = std::stof(tok[6]);
    uint64_t q = std::stoull(tok[7]);

    return order_data(order_id, time_stamp, symb, price, q, ord_side, ord_type);
  }
  catch (const dummy_parse_error&) {
    throw new_parse_error(order_id);
  }
  catch (const std::exception&) {
    throw new_parse_error(order_id);
  }
}

order_data parse_amend_order_string(const std::string& line)
{
  return parse_new_order_string(line);
}

struct cancel_command
{
  uint64_t m_id;
  uint32_t m_timestamp;
};

cancel_command parse_cancel_string(const std::string& line)
{
  auto tok = split_string(line);
  assert(tok.size() == 3);

  auto id = std::stoull(tok[1]);
  auto tim = std::stoul(tok[2]);
  return {id, tim};
}

struct match_command
{
  match_command(uint32_t t)
    :m_time(t)
  {

  }
  match_command() {

  }
  match_command(const std::string& s, uint32_t tim)
    :m_symb(s)
    ,m_time(tim)
  {

  }
  std::string m_symb;
  uint32_t m_time = 0;
};
match_command parse_match_string(const std::string& line)
{
  auto tok = split_string(line);
  if (tok.size() == 2) {
    return match_command{ std::stoul(tok[1]) };
  }
  else if (tok.size() == 3) {
    return match_command(tok[2], std::stoul(tok[1]));
  }
  assert(false);
  return match_command{};
}

query_data parse_query_string(const std::string& s)
{
  auto tok = split_string(s);
  if (tok.empty()) {
    assert(false);
  }
  if (tok.size() == 1) {
    return query_data{};
  }
  if (tok.size() == 2) {
    try {
      uint32_t time = std::stoul(tok[1]);
      return query_data{time};
    }
    catch (...) {
      // if cannot convert to number, then it is symbol name
      return query_data{ tok[1] };
    }
  }
  if (tok.size() == 3) {
    try {
      uint32_t time = std::stoul(tok[1]);
      std::string symb = tok[2];
      return query_data(symb, time);
    }
    catch (...) {
      // if not able to convert to number, try other way around
    }
    try {
      uint32_t time = std::stoul(tok[2]);
      std::string symb = tok[1];
      return query_data(symb, time);
    }
    catch (...) {
      assert(false);
    }
  }
  return query_data{};
}

int main()
{
  //basic_limit_orders_matching_tests();
  //basic_buy_queue_tests();
  //basic_sell_queue_tests();

  //assert(false);

  order_engines engines;

  std::map<uint32_t, order_engines> history;
  
  std::string line;
  // skip first line
  std::getline(std::cin, line);
  while (std::getline(std::cin, line)) {
    if (line.size() == 0) {
      assert(false);
    }
    uint32_t time_stamp = 0;
    try {
      switch (line[0]) {
      case 'N': {
        order_data od = parse_new_order_string(line);
        engines.add_order_from_order_data(od);
        time_stamp = od.get_time();
        break;
      }
      case 'A': {
        order_data od = parse_amend_order_string(line);
        engines.amend_order_from_order_data(od);
        time_stamp = od.get_time();
        break;
      }
      case 'X':
      {
        cancel_command cmd = parse_cancel_string(line);
        engines.cancel_order(cmd.m_id);
        time_stamp = cmd.m_timestamp;
        break;
      }
      case 'M':
      {
        match_command cmd  = parse_match_string(line);
        std::vector<matched_result_detail> result;
        if (!cmd.m_symb.empty()) {
          result = engines.match_one(cmd.m_symb);
        }
        else {
          result = engines.match_all();
        }

        for (const auto& res : result) {
          std::cout << res.to_string() << std::endl;
        }
        time_stamp = cmd.m_time;
        break;
      }
      case 'Q':
      {
        query_data d = parse_query_string(line);
        if (d.get_timestamp() == 0) {
          engines.dump_data(d);
        }
        else {
          //auto iter = history.find(d.get_timestamp());
          //if (iter != history.end()) {
          //  iter->second.dump_data(d);
          //}
          auto iter = history.lower_bound(d.get_timestamp());
          if (iter == history.end()) {
            if (history.size() > 0) {
              --iter;
            }
          }
          else if (iter->first > d.get_timestamp()) {
            --iter;
          }
          if (iter != history.end()) {
            iter->second.dump_data(d);
          }
        }
        break;
      }
      }
    }
    catch (const parse_error& err) {
      std::cout << err.get_msg() << '\n';
    }
    if (time_stamp > 0) {
      history[time_stamp] = engines;
    }
  }

}
