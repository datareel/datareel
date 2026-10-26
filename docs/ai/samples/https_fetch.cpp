// DataReel 7 sample: HTTPS GET and POST with gxsHTTPClient.
// Usage: https_fetch https://host/path [ca.pem]
#include <iostream>
#include <string>
#include "gxshttpc.h"

using namespace datareel;

int main(int argc, char **argv)
{
  if(argc < 2) {
    std::cerr << "Usage: https_fetch https://host/path [ca.pem]\n";
    return 1;
  }
  gxsHTTPClient client; // https:// only; http:// URLs are refused
  client.SetTimeout(30);
  if(argc > 2) client.SetCAFile(argv[2]);
  client.AddHeader("Accept", "application/json, text/plain, */*");

  gxsHTTPMessage resp;
  // Returns 0 when any response arrived, so check resp.status too.
  // Redirects are followed (5 by default).
  if(client.Get(argv[1], resp) != 0) {
    std::cerr << "GET failed: " << client.Error() << "\n";
    return 1;
  }
  std::cout << "GET " << client.FinalURL() << " -> " << resp.status << " "
	    << resp.reason << ", " << resp.body.size() << " bytes, "
	    << "Content-Type: " << resp.Header("content-type") << "\n";

  // POST a JSON body. Request() sends any other method.
  gxsHTTPMessage post_resp;
  if(client.Post(argv[1], "{\"name\":\"DataReel\"}", "application/json",
		 post_resp) != 0) {
    std::cerr << "POST failed: " << client.Error() << "\n";
    return 1;
  }
  std::cout << "POST -> " << post_resp.status << " " << post_resp.reason
	    << "\n";
  return (resp.status >= 200 && resp.status < 300) ? 0 : 2;
}
