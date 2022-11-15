
#include "ypc/stbox/ebyte.h"
#include "ypc/stbox/stx_common.h"
#include "ypc/core_t/analyzer/data_source.h"
#include "ypc/stbox/tsgx/log.h"
#include "user_type.h"

#include "ypc/corecommon/to_type.h"
#include <hpda/extractor/raw_data.h>
#include <hpda/output/memory_output.h>
#include <hpda/processor/query/filter.h>

typedef ff::net::ntpackage<0, name> nt_package_t;

class gdp_prediction_parser {
public:
  gdp_prediction_parser(ypc::data_source<stbox::bytes> *source)
      : m_source(source){};

  inline stbox::bytes do_parse(const stbox::bytes &param) {
    LOG(INFO) << "into converter";
    ypc::to_type<stbox::bytes, gdp_prediction_item_t> converter(m_source);
    LOG(INFO) << "into from_bytes";
    auto pkg = ypc::make_package<nt_package_t>::from_bytes(param);
    LOG(INFO) << "filter_impl";
    hpda::processor::internal::filter_impl<gdp_prediction_item_t> match(
        &converter, [&](const gdp_prediction_item_t &v) {
          std::string first_item = v.get<name>();
          // LOG(INFO) << "first: " << first_item;
          // LOG(INFO) << "input: " << v.get<name>();
          if ( first_item == pkg.get< name >() ) {
            LOG(INFO) << "-->first: " << first_item;
            LOG(INFO) << "-->input: " << v.get<name>();
            return true;
          }
          return false;
        });
    LOG(INFO) << "into mo";
    hpda::output::internal::memory_output_impl<gdp_prediction_item_t> mo(&match);
    LOG(INFO) << "into run";
    mo.get_engine()->run();
    LOG(INFO) << "do parse done";
    bool is_found = false;
    stbox::bytes result("地区名称 ( 区县 ) , 2021年GDP预估 ( 万元 ) \n");
    for (auto it : mo.values()) {
      is_found = true;
      result += stbox::bytes(it.get<name>());
      result += stbox::bytes(",");
      result += stbox::bytes(it.get<gdp>());
      result += stbox::bytes("\n");
    }

    if ( !is_found )
      result = stbox::bytes( "您输入的参数不能匹配到对应的地区, 请重新提交\n" );
    return result;
  }

protected:
  ypc::data_source<stbox::bytes> *m_source;
};

