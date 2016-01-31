FROM ubuntu:14.04

RUN apt-get update && apt-get install -y openssh-server
RUN mkdir /var/run/sshd

RUN useradd -m alice
COPY id_ecdsa /home/alice/.ssh/id_ecdsa
COPY id_ecdsa.pub /home/alice/.ssh/id_ecdsa.pub
COPY id_ecdsa.pub /home/alice/.ssh/authorized_keys
COPY ssh_host_ecdsa_key /etc/ssh/ssh_host_ecdsa_key
COPY ssh_host_ecdsa_key.pub /etc/ssh/ssh_host_ecdsa_key.pub
RUN chmod 600 /home/alice/.ssh/* \
  && chmod 700 /home/alice/.ssh \
  && chown -R alice:alice /home/alice \
  && chmod 600 /etc/ssh/ssh_host_ecdsa_key \
  && chmod 644 /etc/ssh/ssh_host_ecdsa_key.pub

EXPOSE 22
CMD ["/usr/sbin/sshd", "-D", "-E", "/var/log/sshd.log"]
