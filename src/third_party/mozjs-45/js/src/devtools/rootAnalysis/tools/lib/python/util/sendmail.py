import email.utils
from smtplib import SMTP
from email.mime.text import MIMEText


def sendmail(from_, to, subject, body, smtp_server):
    s = SMTP()
    s.connect(smtp_server)

    for addr in to:
        m = MIMEText(body)
        m['date'] = email.utils.formatdate()
        m['to'] = addr
        m['subject'] = subject
        s.sendmail(from_, [addr], m.as_string())

    s.quit()
