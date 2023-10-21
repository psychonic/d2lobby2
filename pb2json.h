/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 shafreeck renenglish at gmail dot com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <jansson.h>
#include <string>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
//using namespace google::protobuf;
char *pb2json(const google::protobuf::Message &msg);
char *pb2json(google::protobuf::Message *msg,const char *buf,int len);
json_t *parse_msg(const google::protobuf::Message *msg);
json_t *parse_repeated_field(const google::protobuf::Message *msg,const google::protobuf::Reflection * ref,const google::protobuf::FieldDescriptor *field);
