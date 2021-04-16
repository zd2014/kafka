#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <string.h>
#include "librdkafka/rdkafkacpp.h"
using namespace std;

bool run = true;
bool exit_eof = true;
class ExampleDeliveryReportCb : public RdKafka::DeliveryReportCb
{
 public:
  void dr_cb (RdKafka::Message &message) {
    std::cout << "Message delivery for (" << message.len() << " bytes): " <<
        message.errstr() << std::endl;
    if (message.key())
      std::cout << "Key: " << *(message.key()) << ";" << std::endl;
  }
};


class ExampleEventCb : public RdKafka::EventCb {
 public:
  void event_cb (RdKafka::Event &event) {
    switch (event.type())
    {
      case RdKafka::Event::EVENT_ERROR:
        std::cerr << "ERROR (" << RdKafka::err2str(event.err()) << "): " <<
            event.str() << std::endl;
        if (event.err() == RdKafka::ERR__ALL_BROKERS_DOWN)
          run = false;
        break;

      case RdKafka::Event::EVENT_STATS:
        std::cerr << "\"STATS\": " << event.str() << std::endl;
        break;

      case RdKafka::Event::EVENT_LOG:
        fprintf(stderr, "LOG-%i-%s: %s\n",
                event.severity(), event.fac().c_str(), event.str().c_str());
        break;

      default:
        std::cerr << "EVENT " << event.type() <<
            " (" << RdKafka::err2str(event.err()) << "): " <<
            event.str() << std::endl;
        break;
    }
  }
};

/* Use of this partitioner is pretty pointless since no key is provided * in the produce() call.so when you need input your key */
class MyHashPartitionerCb : public RdKafka::PartitionerCb {
    public:
        int32_t partitioner_cb (const RdKafka::Topic *topic, const std::string *key,int32_t partition_cnt, void *msg_opaque)
        {
            std::cout<<"partition_cnt="<<partition_cnt<<std::endl;
            return djb_hash(key->c_str(), key->size()) % partition_cnt;
        }
    private:
        static inline unsigned int djb_hash (const char *str, size_t len)
        {
        unsigned int hash = 5381;
        for (size_t i = 0 ; i < len ; i++)
            hash = ((hash << 5) + hash) + str[i];
        std::cout<<"hash1="<<hash<<std::endl;

        return hash;
        }
};

//comsume_cb回调的时候调用msg_consume，处理每一条从kafak得到的消息
void msg_consume(RdKafka::Message* message, void* opaque)
{
    switch (message->err())
    {
        case RdKafka::ERR__TIMED_OUT:
            break;

        case RdKafka::ERR_NO_ERROR:
          /* Real message */
            std::cout << "Read msg at offset " << message->offset() << std::endl;
            if (message->key())
            {
                std::cout << "Key: " << *message->key() << std::endl;
            }
            printf("%.*s\n", static_cast<int>(message->len()),static_cast<const char *>(message->payload()));
            break;
        case RdKafka::ERR__PARTITION_EOF:
              /* Last message */
              if (exit_eof)
              {
                  run = false;
                  cout << "ERR__PARTITION_EOF" << endl;
              }
              break;
        case RdKafka::ERR__UNKNOWN_TOPIC:
        case RdKafka::ERR__UNKNOWN_PARTITION:
            std::cerr << "Consume failed: " << message->errstr() << std::endl;
            run = false;
            break;
    default:
        /* Errors */
        std::cerr << "Consume failed: " << message->errstr() << std::endl;
        run = false;
    }
}

class ExampleConsumeCb : public RdKafka::ConsumeCb {
    public:
        void consume_cb (RdKafka::Message &msg, void *opaque)
        {
            msg_consume(&msg, opaque);
        }
};

void TestConsumer()
{
    std::string brokers = "localhost";
    std::string errstr;
    std::string topic_str="helloworld_kugou_test";//helloworld_kugou
    MyHashPartitionerCb hash_partitioner;
    int32_t partition = RdKafka::Topic::PARTITION_UA;
    //为何不能用？？在Consumer这里只能写0？？？无法自动吗？？？
    partition = 0;
    int64_t start_offset = RdKafka::Topic::OFFSET_BEGINNING;
    bool do_conf_dump = false;
    int opt;

    int use_ccb = 0;

    //Create configuration objects
    RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    RdKafka::Conf *tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);

    if (tconf->set("partitioner_cb", &hash_partitioner, errstr) != RdKafka::Conf::CONF_OK)
    {
        std::cerr << errstr << std::endl;
        exit(1);
    }

    /* * Set configuration properties */
    conf->set("metadata.broker.list", brokers, errstr);
    ExampleEventCb ex_event_cb;
    conf->set("event_cb", &ex_event_cb, errstr);

    ExampleDeliveryReportCb ex_dr_cb;

    /* Set delivery report callback */
    conf->set("dr_cb", &ex_dr_cb, errstr);
    /* * Create consumer using accumulated global configuration. */
    RdKafka::Consumer *consumer = RdKafka::Consumer::create(conf, errstr);
    if (!consumer)
    {
      std::cerr << "Failed to create consumer: " << errstr << std::endl;
      exit(1);
    }

    std::cout << "% Created consumer " << consumer->name() << std::endl;

    /* * Create topic handle. */
    RdKafka::Topic *topic = RdKafka::Topic::create(consumer, topic_str, tconf, errstr);
    if (!topic)
    {
      std::cerr << "Failed to create topic: " << errstr << std::endl;
      exit(1);
    }

    /* * Start consumer for topic+partition at start offset */
    RdKafka::ErrorCode resp = consumer->start(topic, partition, start_offset);
    if (resp != RdKafka::ERR_NO_ERROR) {
      std::cerr << "Failed to start consumer: " << RdKafka::err2str(resp) << std::endl;
      exit(1);
    }

    ExampleConsumeCb ex_consume_cb;

    /* * Consume messages */
    while (run)
    {
        if (use_ccb)
        {
            consumer->consume_callback(topic, partition, 1000, &ex_consume_cb, &use_ccb);
      }
      else
      {
          RdKafka::Message *msg = consumer->consume(topic, partition, 1000);
          msg_consume(msg, NULL);
          delete msg;
      }
      consumer->poll(0);
    }

    /* * Stop consumer */
    consumer->stop(topic, partition);

    consumer->poll(1000);

    delete topic;
    delete consumer;
}
 
int main(int argc, char *argv[])
{
    TestConsumer();
    return EXIT_SUCCESS;
}
