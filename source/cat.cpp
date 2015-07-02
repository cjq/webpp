#include "../header/bookstore.hpp"

namespace bookstore {

BOOKSTORE_DB_CONN()

struct Args {
    std::string cat_id;
};

RPP_TYPE_OBJECT(
    __(cat_id),
    Args
)

void exec(cgicc::FCgiCC<> &cgi) {
    Args args;
    ajaxArgs(cgi, args);

    // find

    using namespace bsoncxx::builder::stream;

    auto cursor = db_cat.find(
        document{}
            << "_id" << args.cat_id << finalize
    );

    // get data

    Cat result;

    RPP_META_DYNAMIC(
        "result", Cat, VisitorListDB
    ) result_meta{result};

    rpp::VisitorBSONView<> result_visitor{
        bsoncxx::types::b_document{*(cursor.begin())}
    };
    result_meta.doVisit(result_visitor);

    ajaxReturn(cgi, result);
}

}

BOOKSTORE_MAIN(exec, ignoreErr)
