#include "controllers/AuthFilter.h"

void AuthFilter::doFilter(const HttpRequestPtr &req, FilterCallback &&fcb,
                          FilterChainCallback &&fccb) {
  if (req->session()->find("user_id")) {
    fccb();
    return;
  }
  auto res = HttpResponse::newHttpResponse();
  res->setStatusCode(k401Unauthorized);
  fcb(res);
}
