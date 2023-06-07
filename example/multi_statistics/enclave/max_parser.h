#include "common_type.h"
#include "ypc/core_t/analyzer/data_source.h"
#include "ypc/corecommon/data_source.h"
#include "ypc/corecommon/package.h"
#include "ypc/corecommon/to_type.h"
#include "ypc/stbox/ebyte.h"
#include "ypc/stbox/stx_common.h"
#include "ypc/stbox/tsgx/log.h"
#include <hpda/extractor/raw_data.h>
#include <hpda/output/memory_output.h>
#include <hpda/processor/query/filter.h>
#include <hpda/processor/transform/concat.h>
#include <string.h>

class max_parser {
public:
  max_parser() {}
  max_parser(std::vector<std::shared_ptr<ypc::data_source_with_dhash>> &source)
      : m_datasources(source){};

  inline stbox::bytes do_parse(const stbox::bytes &param) {
    LOG(INFO) << "do parse";
    int counter = 0;
    if (m_datasources.size() == 0) {
      return stbox::bytes("no data source");
    }

    typedef ypc::nt<stbox::bytes> ntt;
    hpda::processor::concat<ntt::data> concator(m_datasources[0].get());
    for (size_t i = 1; i < m_datasources.size(); i++) {
      concator.add_upper_stream(m_datasources[i].get());
    }
    ypc::to_type<stbox::bytes, value_item_t> converter(&concator);
    hpda::processor::internal::filter_impl<value_item_t> match(
        &converter, [&](const value_item_t &v) { return true; });

    hpda::output::internal::memory_output_impl<value_item_t> mo(&match);
    mo.get_engine()->run();
    LOG(INFO) << "do parse done";

    int max_val = 0;
    for (const auto &it : mo.values()) {
      max_val = std::max(max_val, it.get<::value>());
    }
    return stbox::bytes(std::to_string(max_val));
  }

protected:
  std::vector<std::shared_ptr<ypc::data_source_with_dhash>> m_datasources;
};