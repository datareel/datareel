// DataReel 7 sample: send a message through a mail server's
// submission port (587) with STARTTLS and a login.
// Usage: send_mail host port user password from to [ca.pem]
#include <cstdlib>
#include <iostream>
#include <string>
#include "gxsmtp.h"

using namespace datareel;

int main(int argc, char **argv)
{
  if(argc < 7) {
    std::cerr << "Usage: send_mail host port user password from to [ca.pem]\n";
    return 1;
  }
  gxsSMTPClient smtp;
  smtp.SetTimeout(30);
  if(argc > 7) smtp.SetCAFile(argv[7]);
  // Port 587 (submission) uses gxsMAIL_STARTTLS; port 465 uses
  // gxsMAIL_IMPLICIT_TLS, the default. A server that does not offer STARTTLS is
  // refused; nothing is ever sent in plain text.
  if(smtp.Connect(argv[1], std::atoi(argv[2]), gxsMAIL_STARTTLS) != 0 ||
     smtp.Login(argv[3], argv[4]) != 0) {
    std::cerr << "SMTP: " << smtp.Error() << "\n";
    return 1;
  }
  if(smtp.SendMessage(argv[5], { argv[6] }, "Test from DataReel 7",
		      "This message was sent over TLS.\n") != 0) {
    std::cerr << "Send failed: " << smtp.Error() << "\n";
    return 1;
  }
  smtp.Quit();
  std::cout << "Sent\n";
  return 0;
}
