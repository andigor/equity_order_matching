
#include <iostream>
#include <map>
#include <cassert>

struct order_data_key
{
  uint32_t m_time;
  float m_price;
};

class order_data
{
public:
  order_data(uint64_t id, uint32_t tim, const std::string& nam, float price, uint64_t q)
  : m_id(id)
  , m_time(tim)
  , m_symbol(nam)
  , m_price(price)
  , m_q(q)
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
private:
  uint64_t m_id;
  uint32_t m_time;
  std::string m_symbol;
  float m_price;
  uint64_t m_q;
};

enum class order_side{
    sell
  , buy
};

template <order_side OrderSide>
class order
{
public:
  order(const order_data& d)
    :m_order_data(d)
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
private:
  order_data m_order_data;
};

class limit_order_sell : public order<order_side::sell>
{
public:
  limit_order_sell(const order_data& od)
    :order(od)
  {

  }
};

class limit_order_buy : public order<order_side::buy>
{
public:
  limit_order_buy(const order_data& od)
    :order(od)
  {

  }
};


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
  void put_order(const Ord& o)
  {
    this->insert(std::make_pair(o.get_key(), o));
  }
};

class limit_order_buy_queue: public limit_order_queue<limit_order_buy, limit_order_buy_less>
{
public:
};

class limit_order_sell_queue: public limit_order_queue<limit_order_sell, limit_order_sell_less>
{
public:
};


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

int main()
{

  basic_limit_orders_matching_tests();
  basic_buy_queue_tests();
}
