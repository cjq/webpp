#pragma once

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>

#include "reflection++/visitor/strtree.hpp"
#include "reflection++/visitor/json.hpp"
#include "reflection++/visitor/bson.hpp"
#include "reflection++/visitor/bson_view.hpp"
#include "reflection++/meta.hpp"

#include "bookstore_hash.hpp"
#include "bookstore_cgi.hpp"

namespace bookstore {

#ifndef BOOKSTORE_RPP_READY
    #define BOOKSTORE_RPP_READY
    RPP_ACCESSOR_INFER_INIT()
#endif

// helper macros

#define BOOKSTORE_DB_CONN() \
    mongocxx::instance inst{}; \
    mongocxx::client conn{}; \
    \
    auto db = conn["bookstore"]; \
    auto db_cat = db["cat"]; \
    auto db_book = db["book"]; \
    auto db_user = db["user"]; \
    auto db_buy = db["buy"]; \
    auto db_session = db["session"];

#define BOOKSTORE_MAIN(Exec, Err) \
    int main() { \
        using namespace bookstore; \
        webutil::fcgiccExec((Exec), (Err)); \
        \
        return 0; \
    }

#define BOOKSTORE_EXEC_ENTER(SObj, AObj) \
    Session (SObj); \
    makeSession(cgi, db_session, (SObj)); \
    Args (AObj); \
    ajaxArgs(cgi, (AObj));

#define BOOKSTORE_EXEC_EXIT(RObj, SObj) \
    ajaxReturn(cgi, (SObj), (RObj)); \
    saveSession(db_session, (SObj)); \
    return;

// visitors

using VisitorListArgs = rpp::TypeList<
    rpp::VisitorIStrTree<cgicc::FCgiCC<>>
>;
using VisitorListJSON = rpp::TypeList<
    rpp::VisitorJSON<cgicc::FCgiCC<>>
>;
using VisitorListDB = rpp::TypeList<
    rpp::VisitorBSON<>,
    rpp::VisitorBSONView<>
>;

// database io

template <class T>
void bsonGet(const bsoncxx::document::view &view, T &value) {
    RPP_META_DYNAMIC(
        "data", T, VisitorListDB
    ) meta{value};

    rpp::VisitorBSONView<> visitor{
        bsoncxx::types::b_document{view}
    };
    meta.doVisit(visitor);
}

template <class T, class Filter>
bool dbGetOneF(
    mongocxx::collection &db,
    T &value,
    const Filter &filter,
    bool force = true
) {
    auto cursor = db.find(filter);

    auto iter = cursor.begin();
    if (iter != cursor.end()) {
        bsonGet(*iter, value);
        return true;
    } else {
        if (force) {
            throw std::exception{};
        } else {
            return false;
        }
    }
}

template <class T>
bool dbGetOne(
    mongocxx::collection &db,
    T &value,
    const std::string &id,
    bool force = true
) {
    using namespace bsoncxx::builder::stream;

    return dbGetOneF(
        db,
        value,
        document{}
            << "_id" << id << finalize,
        force
    );
}

template <class T>
rpp::VisitorBSON<> bsonGen(T &&value) {
    RPP_META_DYNAMIC(
        "data", T, VisitorListDB
    ) meta{value};

    rpp::VisitorBSON<> visitor{};
    meta.doVisit(visitor);

    return std::move(visitor);
}

template <class T>
std::string dbInsert(mongocxx::collection &db, T &&value) {
    std::string id{value._id};

    db.insert_one(bsonGen(value).view());

    return id;
}

template <class T>
void dbReplace(mongocxx::collection &db, T &value) {
    using namespace bsoncxx::builder::stream;

    db.replace_one(
        document{}
            << "_id" << value._id << finalize,
        bsonGen(value).view()
    );
}

// session

template <class Session>
void saveSession(mongocxx::collection &db, Session &session) {
    dbReplace(db, session);
}

template <class Session>
bool getSession(
    cgicc::FCgiCC<> &cgi,
    mongocxx::collection &db,
    Session &session
) {
    // parse cookies
    std::string id;
    std::string key;

    for (const auto &item: cgi.getEnvironment().getCookieList()) {
        std::string name = item.getName();
        if (name == session_tag_id) {
            id = item.getValue();
        } else if (name == session_tag_key) {
            key = item.getValue();
        }
    }

    // find session in the database
    if (id != "") {
        using namespace bsoncxx::builder::stream;

        auto cursor = db.find(
            document{}
                << "_id" << id
                << "key" << key
                << "$lt" << open_document
                    << "date_create" << (
                        time(nullptr) + 3600 * 24
                    ) << close_document << finalize
                // TODO: timeout
        );

        auto iter = cursor.begin();
        if (iter != cursor.end()) {
            bsonGet(*iter, session);
            return true;
        }
    }

    return false;
}

template <class Session>
void makeSession(
    cgicc::FCgiCC<> &cgi,
    mongocxx::collection &db,
    Session &session
) {
    try {
        if (getSession(cgi, db, session)) {
            // ok
            return;
        }
    } catch (...) {}

    session = Session{
        genOID(), randStr(),
        nullptr, nullptr, nullptr, false,
        time(nullptr)
    };
    dbInsert(db, session);
}

// ajax

template <class Args>
void ajaxArgs(cgicc::FCgiCC<> &cgi, Args &args) {
    RPP_META_DYNAMIC(
        "args", Args, VisitorListArgs
    ) meta{args};

    rpp::VisitorIStrTree<cgicc::FCgiCC<>> visitor{cgi};
    meta.doVisit(visitor);
}

template <class Session, class Result>
void ajaxReturn(
    cgicc::FCgiCC<> &cgi,
    const Session &session,
    Result &result
) {
    // header
    cgicc::HTTPContentHeader header{content_type_json};
    header.setCookie(cgicc::HTTPCookie{session_tag_id, session._id});
    header.setCookie(cgicc::HTTPCookie{session_tag_key, session.key});
    cgi << header;

    // content
    RPP_META_DYNAMIC(
        "result", Result, VisitorListJSON
    ) meta{result};

    rpp::VisitorJSON<cgicc::FCgiCC<>> visitor{cgi};
    meta.doVisit(visitor);
};

}
