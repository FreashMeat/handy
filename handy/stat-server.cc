#include "http.h"
#include "logging.h"
#include "stat-server.h"

using namespace std;

namespace handy {

static string query_link(const string& path) {
    return util::format("<a href=\"/?stat=%s\">%s</a>", path.c_str(), path.c_str());
}

static string page_link(const string& path) {
    return util::format("<a href=\"/%s\">%s</a>", path.c_str(), path.c_str());
}

StatServer::StatServer(EventBase* base, Ip4Addr addr): server_(base, addr) {
    server_.onDefault([this](const HttpConn& con) {
        HttpRequest& req = con.getRequest();
        HttpResponse resp;
        Buffer buf;
        string query = req.getQuery("stat");
        if (query.empty()) {
            query.assign(req.uri.data()+1, req.uri.size()-1);
        }
        if (query.size()) {
            auto p = allcbs_.find(query);
            if (p != allcbs_.end()) {
                p->second(req, resp);
            } else {
                resp.setNotFound();
            }
        }
        if (req.uri == "/") {
            buf.append("<a href=\"/\">refresh</a><br/>\n");
            buf.append("<table>\n");
            buf.append("<tr><td>Stat</td><td>Desc</td><td>Value</td></tr>\n");
            for (auto& stat: statcbs_) {
                HttpResponse r;
                req.uri = stat.first;
                stat.second.second(req, r);
                buf.append("<tr><td>").append(page_link(stat.first))
                    .append("</td><td>").append(stat.second.first)
                    .append("</td><td>").append(r.body)
                    .append("</td></tr>\n");
            }
            buf.append("</table>\n<br/>\n<table>\n")
                .append("<tr><td>Page</td><td>Desc</td>\n");
            for (auto& stat: pagecbs_) {
                buf.append("<tr><td>").append(page_link(stat.first))
                    .append("</td><td>").append(stat.second.first)
                    .append("</td></tr>\n");
            }
            buf.append("</table>\n<br/>\n<table>\n")
                .append("<tr><td>Cmd</td><td>Desc</td>\n");
            for (auto& stat: cmdcbs_) {
                buf.append("<tr><td>").append(query_link(stat.first))
                    .append("</td><td>").append(stat.second.first)
                    .append("</td></tr>\n");
            }
            buf.append("</table>\n");
            if (resp.body.size()) {
                buf.append(util::format("<br/>SubQuery %s:<br/> %s", query.c_str(), resp.body.c_str()));
            }
            resp.body = Slice(buf.data(), buf.size());
       }
        info("response is: %d \n%.*s", resp.status, (int)resp.body.size(), resp.body.data());
        con.send(resp);
    });
}

void StatServer::onRequest(StatType type, const string& key, const string& desc, const StatCallBack& cb){
    if (type == STATE) {
        statcbs_[key] = { desc, cb };
    } else if (type == PAGE) {
        pagecbs_[key] = { desc, cb };
    } else if (type == CMD) {
        cmdcbs_[key] = { desc, cb};
    } else {
        error("unknow state type: %d", type);
        return;
    }
    allcbs_[key] = cb;
}

void StatServer::onRequest(StatType type, const string& key, const string& desc, const InfoCallBack& cb) {
    onRequest(type, key, desc, [cb](const HttpRequest&, HttpResponse& r) {
        r.body = cb();
    });
}


}