#kafka相关学习记录
官方文档：https://github.com/edenhill/librdkafka
1. kafka是什么
   发布订阅式的消息系统(存储消息，实时处理)。消息按topic进行划分存储，一个消费者可以订阅一个或者多个topic.一条消息可以被多个消费者消费。kafak保存所有发布的消息，不管消息有没有被消费过。kafaka提供可配置的保留策略去删除旧数据。kafak的性能与数据量大小无关，所有数据保存久一点也没有关系。

2. kafka架构（kafak消息怎么存储，怎么读写）
   kafak是一个集群。
   重要：https://www.cnblogs.com/sujing/p/10960832.html
   kafka cluster：
   produce: 消息的产生者
　 Broker：Broker是kafka实例，每个服务器上有一个或多个kafka的实例，我们姑且认为每个broker对应一台服务器。每个kafka集群内的broker都有一个不重复的编号，如图中的broker-0、broker-1等……
　　Topic：消息的主题，可以理解为消息的分类，kafka的数据就保存在topic。在每个broker上都可以创建多个topic。
　　Partition：Topic的分区，每个topic可以有多个分区，分区的作用是做负载，提高kafka的吞吐量。同一个topic在不同的分区的数据是不重复的，partition的表现形式就是一个一个的文件夹！
　　Replication:每一个分区都有多个副本，副本的作用是做备胎。当主分区（Leader）故障的时候会选择一个备胎（Follower）上位，成为Leader。在kafka中默认副本的最大数量是10个，且副本的数量不能大于Broker的数量，follower和leader绝对是在不同的机器，同一机器对同一个分区也只可能存放一个副本（包括自己）。
　　Message：每一条发送的消息主体。
　　Consumer：消费者，即消息的消费方，是消息的出口。
　　Consumer Group：我们可以将多个消费组组成一个消费者组，在kafka的设计中同一个分区的数据只能被消费者组中的某一个消费者消费。同一个消费者组的消费者可以消费同一个topic的不同分区的数据，这也是为了提高kafka的吞吐量！
　　Zookeeper：kafka集群依赖zookeeper来保存集群的的元信息，来保证系统的可用性。

3. kafka数据怎么存储
4. produce怎么生产数据到kafka (保证消息不丢)
    生产数据需要指定borker, topic：
    ./kafka-console-producer.sh --broker-list localhost:9092 --topic flume
    在kafka中，如果某个topic有多个partition，producer又怎么知道该将数据发往哪个partition呢？kafka中有几个原则：
　　1、 partition在写入的时候可以指定需要写入的partition，如果有指定，则写入对应的partition。
　　2、 如果没有指定partition，但是设置了数据的key，则会根据key的值hash出一个partition。
　　3、 如果既没指定partition，又没有设置key，则会轮询选出一个partition。

　　保证消息不丢失是一个消息队列中间件的基本保证，那producer在向kafka写入消息的时候，怎么保证消息不丢失呢？其实上面的写入流程图中有描述出来，那就是通过ACK应答机制！在生产者向队列写入数据的时候可以设置参数来确定是否确认kafka接收到数据，这个参数可设置的值为0、1、all。
　　0代表producer往集群发送数据不需要等到集群的返回，不确保消息发送成功。安全性最低但是效率最高。
　　1代表producer往集群发送数据只要leader应答就可以发送下一条，只确保leader发送成功。
　　all代表producer往集群发送数据需要所有的follower都完成从leader的同步才会发送下一条，确保leader发送成功和所有的副本都完成备份。安全性最高，但是效率最低。

　　最后要注意的是，如果往不存在的topic写数据，能不能写入成功呢？kafka会自动创建topic，分区和副本的数量根据默认配置都是1。(the broker configuration property auto.create.topics.enable=true is set.)
    
    发送消息有三种方式：
    1、发送并忘记( fire-and-forget)：我们把消息发送给服务器，但井不关心它是否正常到达。大多数情况下，消息会正常到达，因为 Kafka是高可用的，而且生产者会自动尝试重发。不过，使用这种方式有时候也会丢失一些消息。
    2、同步发送：我们使用send()方怯发送消息， 它会返回一个Future对象，调用get()方法进行等待， 就可以知道悄息是否发送成功。
    3、异步发送：我们调用 send() 方怯，并指定一个回调函数， 服务器在返回响应时调用该函数。
    
    多生产者生产数据

5. 消费者怎么从kafka消费数据
   1、需要找到数据所在的broker, 找到后消费者与broker建立tcp链接:
    https://www.huaweicloud.com/articles/9390ecaf1e35bd52342243c9cc977442.html
   2、三种消费方式（至多一次，最少一次，恰好消费一次）
   消费可能出现的问题：
   如果消息提交了，但是消费失败了，那消费消息就会丢
   如果消息消费成功了之后，再commit, commit失败了，就会出现重复消费。
   就对应到三种消费模式：
   at most onece模式
   基本思想是保证每一条消息commit成功之后，再进行消费处理；
   设置自动提交为false，接收到消息之后，首先commit，然后再进行消费
   at least onece模式
   基本思想是保证每一条消息处理成功之后，再进行commit；
   设置自动提交为false；消息处理成功之后，手动进行commit；
   采用这种模式时，最好保证消费操作的“幂等性”，防止重复消费；
   exactly onece模式
   核心思想是将offset作为唯一id与消息同时处理，并且保证处理的原子性；
   设置自动提交为false；消息处理成功之后再提交；
   比如对于关系型数据库来说，可以将id设置为消息处理结果的唯一索引，再次处理时，如果发现该索引已经存在，那么就不处理；

6.api学习
kafka参数配置：
# 异步模式下缓冲数据的最大时间。例如设置为100则会集合100ms内的消息后发送，这样会提高吞吐量，但是会增加消息发送的延时,对于提高吞吐，建议是大有50ms
queue.buffering.max.ms = 5000
# 异步模式下缓冲的最大消息数，同上
queue.buffering.max.messages = 10000
# 异步模式下，消息进入队列的等待时间。若是设置为0，则消息不等待，如果进入不了队列，则直接被抛弃
queue.enqueue.timeout.ms = -1
# 异步模式下，每次发送的消息数，当queue.buffering.max.messages或queue.buffering.max.ms满足条件之一时producer会触发发送。
batch.num.messages=200 (rd_kafka_conf_t设置)

librakafka配置保证可靠性
request.required.acks and message.send.max.retries

poll()函数作用：
https://blog.csdn.net/sinat_36304757/article/details/106688581

可以设置生产者幂等性：enable.idempotence

小例子：https://www.cnblogs.com/kaishan1990/p/7228683.html
librdkafka文档：https://docs.confluent.io/2.0.0/clients/librdkafka/pages.html

