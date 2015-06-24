#include "reflection++/visitor/strtree.hpp"
#include "reflection++/visitor/json.hpp"
#include "reflection++/accessor_infer.hpp"
#include "reflection++/meta.hpp"

#include "cgiutil.hpp"

#include "cgicc/HTTPHTMLHeader.h"
#include "cgicc/HTMLClasses.h"

namespace demo {

RPP_ACCESSOR_INFER_INIT()

struct Person {
    std::string name;
    int age;
};

RPP_TYPE_OBJECT(
    Person,
    __(name)
    __(age)
)

void exec() {
    FCGX_Request request;

    FCGX_Init();
    FCGX_InitRequest(&request, 0, 0);

    while (FCGX_Accept_r(&request) == 0) {
        try {
            FCgiCC<> cgi{request, std::cin, std::cout, std::cerr};

            cgi << "Content-Type: application/json; charset=utf-8\r\n\r\n";

            Person person{"hcz", 20};

            rpp::MetaImpl<
                rpp::TypeList<
                    rpp::VisitorIStrTree<FCgiCC<> &>,
                    rpp::VisitorJSON<FCgiCC<>>
                >,
                RPP_ACCESSOR_GET(
                    RPP_HOLDER_STR("me"),
                    RPP_HOLDER_DYNAMIC(Person)
                )
            > meta{person};

            rpp::VisitorIStrTree<FCgiCC<> &> v_arg{cgi};
            meta.doVisit(v_arg);
            rpp::VisitorJSON<FCgiCC<>> v_json{cgi};
            meta.doVisit(v_json);

            // cgi << cgicc::HTTPHTMLHeader() << std::endl;

            // cgi << cgicc::html() << std::endl;
            // cgi << cgicc::body() << std::endl;

            // cgi << "Hello<br />";

            // cgicc::form_iterator name = cgi.getElement("name");
            // if (name != cgi.getElements().end()) {
            //     cgi << "Your name: " << **name << std::endl;
            // }

            // cgi << cgicc::body() << cgicc::html() << std::endl;
        } catch (const std::exception &e) {
            //
        }
    }
}

}

int main() {
    demo::exec();

    return 0;
}
