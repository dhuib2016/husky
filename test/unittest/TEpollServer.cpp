#include "src/EpollServer.hpp"
#include "gtest/gtest.h"

using namespace Husky;
class ReqHandler: public IRequestHandler
{
    public:
        virtual ~ReqHandler(){}
    public:
        bool do_GET(const HttpReqInfo& httpReq, string& res) const
        {
            res << httpReq;
            return true;
        }
};


TEST(EpollServerTest, Test1)
{
    ReqHandler handler;
    EpollServer server(22222, &handler);
    ASSERT_TRUE(server);
    ASSERT_TRUE(server.start());
}
