
#include <iostream>
#include <map>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <array>
#include <string>

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
  int get_time()           const { return m_time; }
  std::string get_symbol() const { return m_symbol; }
  float get_price()        const { return m_price; }
  uint64_t get_q()         const { return m_q; }

  void reduce_q(uint64_t q)
  {
    assert(q <= m_q);
    m_q -= q;
  }
  void reset_q()
  {
    m_q = 0;
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
private:
  uint64_t m_id;
  uint32_t m_time;
  std::string m_symbol;
  float m_price;
  uint64_t m_q;
  order_side m_order_side;
  order_type m_order_type;
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
    if (k1.m_price > k2.m_price) {
      return false;
    }
    if (k1.m_price < k2.m_price) {
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
    limit_order_buy order_buy({1, 1, "a", 1.0f, 1});
    limit_order_sell order_sell({1, 1, "a", 1.0f, 1});

    auto res = match_limit_orders(order_buy, order_sell);

    assert(res.m_count_buy == 1);
    assert(res.m_count_sell == 1);
  }
  {
    limit_order_buy order_buy({1, 1, "a", 2.0f, 1});
    limit_order_sell order_sell({1, 1, "a", 1.0f, 1});

    auto res = match_limit_orders(order_buy, order_sell);

    assert(res.m_count_buy == 1);
    assert(res.m_count_sell == 1);
  }
  {
    limit_order_buy order_buy({1, 1, "a", 1.0f, 1});
    limit_order_sell order_sell({1, 1, "a", 2.0f, 1});

    auto res = match_limit_orders(order_buy, order_sell);

    assert(res.m_count_buy == 0);
    assert(res.m_count_sell == 0);
  }
  {
    limit_order_buy order_buy({1, 1, "a", 2.0f, 2});
    limit_order_sell order_sell({1, 1, "a", 1.0f, 1});

    auto res = match_limit_orders(order_buy, order_sell);

    assert(res.m_count_buy == 0);
    assert(res.m_count_sell == 1);

    assert(order_buy.get_q() == 1);
    assert(order_sell.get_q() == 0);
  }
  {
    limit_order_buy order_buy({1, 1, "a", 2.0f, 1});
    limit_order_sell order_sell({1, 1, "a", 1.0f, 2});

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
    this->erase(iter);
  }
private:
  using queue_iter = std::multimap<order_data_key, Ord, Cmp>::iterator;
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


template <class Ord>
class market_order_cont : public std::unordered_map<uint64_t, Ord>
{
public:
  bool id_exists(uint64_t id) const
  {
    auto cnt = this->count(id);
    if (cnt > 0) {
      return true;
    }
    return false;
  }

  bool put_order(const Ord& o)
  {
    auto ret = this->insert(std::make_pair(o.get_id(), o));
    return ret.second;
  }
};

using market_order_buy_cont = market_order_cont<limit_order_buy>;
using market_order_sell_cont = market_order_cont<limit_order_sell>;

void basic_buy_queue_tests()
{
  {
    limit_order_buy_queue bq;
    limit_order_buy b1({ 1, 1, "n", 1.0f, 1 });
    bq.put_order(b1);

    limit_order_buy b2({ 2, 1, "n", 1.0f, 1 });
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
    limit_order_buy b1({ 1, 2, "n", 1.0f, 1 });
    bq.put_order(b1);

    limit_order_buy b2({ 2, 1, "n", 1.0f, 1 });
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
    limit_order_buy b1({ 1, 1, "n", 2.0f, 1 });
    bq.put_order(b1);

    limit_order_buy b2({ 2, 1, "n", 1.0f, 1 });
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

void basic_sell_queue_tests()
{
  {
    limit_order_sell_queue bq;
    limit_order_sell b1({ 1, 1, "n", 1.0f, 1 });
    bq.put_order(b1);

    limit_order_sell b2({ 2, 1, "n", 1.0f, 1 });
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
    limit_order_sell b1({ 1, 2, "n", 1.0f, 1 });
    bq.put_order(b1);

    limit_order_sell b2({ 2, 1, "n", 1.0f, 1 });
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
    limit_order_sell b1({ 1, 1, "n", 2.0f, 1 });
    bq.put_order(b1);

    limit_order_sell b2({ 2, 1, "n", 1.0f, 1 });
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

enum class order_type : int {
    market
  , limit
  , limit_ioc
};

template <typename E>
constexpr auto to_underlying(E e) noexcept
{
  return static_cast<std::underlying_type_t<E>>(e);
}

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

    auto ret = m_put_func.at(to_underlying(ot)).at(to_underlying(os))(d);
 
    return ret;
  }
  void run_matching()
  {
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
  }
private:
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

//order_data

int main()
{
  basic_limit_orders_matching_tests();
  basic_buy_queue_tests();
  basic_sell_queue_tests();

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.size() == 0) {
      assert(false);
    }
    switch (line[0]) {
    case 'N':
      break;
    case 'A':
      break;
    case 'X':
      break;
    case 'M':
      break;
    case 'Q':
      break;
    }
  }

  order_engine eng;
}
