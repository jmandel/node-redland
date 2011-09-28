#include <cstdlib>
#include <node.h>
#include <node_buffer.h>
#include <stdio.h>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include <redland.h>
#include <librdf.h>
#include <raptor2.h>
#include <rasqal.h>

using namespace std;
using namespace v8;
using namespace node;


string ObjectToString(Local<Value> value) {
  String::Utf8Value utf8_value(value);
  return string(*utf8_value);
}

librdf_world* world;
librdf_statement* statement;
raptor_world *raptor_world_ptr;
pthread_mutex_t mutex;

class Model : public ObjectWrap{
  static Persistent<FunctionTemplate> model_ctor;
  librdf_model* m;
  //    int x, y;

public:
  //    Model() :  {}

    static void Initialize(Handle<Object> target) {
        HandleScope scope;

        Local<FunctionTemplate> t = FunctionTemplate::New(NewModel);
        model_ctor = Persistent<FunctionTemplate>::New(t);
        model_ctor->InstanceTemplate()->SetInternalFieldCount(1);
        model_ctor->SetClassName(String::NewSymbol("Model"));

        NODE_SET_PROTOTYPE_METHOD(model_ctor, "parseFromString", ParseFromString);
	NODE_SET_PROTOTYPE_METHOD(model_ctor, "serialize", Serialize);
        target->Set(String::NewSymbol("Model"), model_ctor->GetFunction());
    }

    static Handle<Value> NewModel(const Arguments &args) {

        HandleScope scope;
        Model *model = new Model();
	
	pthread_mutex_lock(&mutex);
	librdf_storage* storage = librdf_new_storage(world, "memory", "test", NULL);
	model->m =librdf_new_model(world, storage, NULL);
	pthread_mutex_unlock(&mutex);

        model->Wrap(args.This());

        return args.This();
    }

    struct model_parse_request {
      Persistent<Function> cb;
      string string_to_parse;
      Model *model;
      string result;
      string format;
    };

    static int EIO_ModelParse(eio_req *req) {
      pthread_mutex_lock(&mutex);

        model_parse_request *model_parse_req = (model_parse_request *)req->data;
	librdf_uri* uri=librdf_new_uri(world, (const unsigned char*)"http://example.librdf.org/");
	librdf_parser* parser=librdf_new_parser(world, model_parse_req->format.c_str(), NULL, NULL);

	librdf_parser_parse_string_into_model(parser, 
					      reinterpret_cast<const unsigned char*>(model_parse_req->string_to_parse.c_str()) , 
					      uri, 
					      model_parse_req->model->m);	

	librdf_free_parser(parser); parser=NULL;  
	librdf_free_uri(uri); uri=NULL;

	model_parse_req->result = (char*) "success";
        pthread_mutex_unlock(&mutex);

	return 0;
    }

  /*
    static int EIO_ModelAddStatement(eio_req *req) {
        model_parse_request *model_parse_req = (model_parse_request *)req->data;

	librdf_statement* statement=librdf_new_statement(world);
		
	librdf_statement_set_subject(statement, 
				     librdf_new_node_from_uri_string(world, (const unsigned char*)"http://example.org/subject"));
	
	librdf_statement_set_predicate(statement,
				       librdf_new_node_from_uri_string(world, (const unsigned char*)"http://example.org/pred1"));
	
	librdf_statement_set_object(statement,
				    librdf_new_node_from_literal(world, (const unsigned char*)"object", NULL, 0));
	
	librdf_model_add_statement(model_parse_req->model->m, statement);
	librdf_free_statement(statement);	

	model_parse_req->result  = (char*)"success";
	return 0;
    }
  */
	
    static int EIO_ModelSerialize(eio_req *req) {
      pthread_mutex_lock(&mutex);

      //	printf("In serializer");
        model_parse_request *model_parse_req = (model_parse_request *)req->data;

	librdf_serializer *serializer = librdf_new_serializer(world, NULL, model_parse_req->format.c_str() ,NULL);
	//	printf("initializec");
	librdf_serializer_set_namespace(serializer, 
					librdf_new_uri (world, (const unsigned char*)"http://smartplatforms.org/terms#"),
					(const char*) ( "sp"));

	librdf_serializer_set_namespace(serializer, 
					librdf_new_uri (world, (const unsigned char*)"http://purl.org/dc/elements/1.1/"),
					(const char*) ( "dc"));

	//	printf("namespaced");
	model_parse_req->result = (char*) librdf_serializer_serialize_model_to_string(serializer, 
										      NULL, 
										      model_parse_req->model->m);
	//	printf("made %s", model_parse_req->result);
	librdf_free_serializer(serializer);
        pthread_mutex_unlock(&mutex);

        return 0;
    }

    static int EIO_ModelAfter(eio_req *req) {
        HandleScope scope;

        ev_unref(EV_DEFAULT_UC);
        model_parse_request *model_parse_req = (model_parse_request *)req->data;
	//	printf("after eio %s", model_parse_req->result);

        Local<Value> argv[1];
        argv[0] = String::New(model_parse_req->result.c_str());

        TryCatch try_catch;

        model_parse_req->cb->Call(Context::GetCurrent()->Global(), 1, argv);

        if (try_catch.HasCaught())
            FatalException(try_catch);

        model_parse_req->cb.Dispose();
        model_parse_req->model->Unref();

	delete model_parse_req;
	
        return 0;
    }

    static Handle<Value> ParseFromString(const Arguments &args) {
        HandleScope scope;


        Model *model = ObjectWrap::Unwrap<Model>(args.This());

        Local<String>   s  = Local<String>::Cast(args[0]);
        Persistent<Function> cb = Persistent<Function>::New(Local<Function>::Cast(args[1]));

        model_parse_request *model_parse_req = new model_parse_request;

	model_parse_req->string_to_parse = ObjectToString(s);

        model_parse_req->cb = cb;
        model_parse_req->model = model;
        model_parse_req->format = "rdfxml";


        eio_custom(EIO_ModelParse, EIO_PRI_DEFAULT, EIO_ModelAfter, model_parse_req);

        ev_ref(EV_DEFAULT_UC);
        model->Ref();

        return Undefined();
    }

    static Handle<Value> Serialize(const Arguments &args) {
        HandleScope scope;
	//	printf("serialzing.");
        Model *model = ObjectWrap::Unwrap<Model>(args.This());

        Local<String> s  = Local<String>::Cast(args[0]);
        Persistent<Function> cb = Persistent<Function>::New(Local<Function>::Cast(args[1]));

        model_parse_request *model_parse_req = new model_parse_request;
        model_parse_req->model = model;

	model_parse_req->format = ObjectToString(s);

	//*format;
        model_parse_req->cb = cb;
	//	printf("serialzing.");
        eio_custom(EIO_ModelSerialize, EIO_PRI_DEFAULT, EIO_ModelAfter, model_parse_req);

        ev_ref(EV_DEFAULT_UC);
        model->Ref();

        return Undefined();
    }
};



Persistent<FunctionTemplate> Model::model_ctor;

static Handle<Value> PrintResult (const Arguments& args) {
  HandleScope scope;
  return String::New("A fake result.");// parsed_result_string;

  /*
  const char *usage = "usage: doSomething(x, y, name, cb)";
  if (args.Length() != 4) {
    return ThrowException(Exception::Error(String::New(usage)));
  }
  int x = args[0]->Int32Value();
  int y = args[1]->Int32Value();
  String::Utf8Value name(args[2]);
  Local<Function> cb = Local<Function>::Cast(args[3]);
  */

};


extern "C" void
init(Handle<Object> target)
{
    HandleScope scope;

  world=librdf_new_world();
  librdf_world_open(world);
  raptor_world_ptr = librdf_world_get_raptor(world);

  if(pthread_mutex_init(&mutex, NULL))
    {
      printf("Unable to initialize a mutex\n");
      return;
    }
  

  NODE_SET_METHOD(target, "print_result", PrintResult);
  Model::Initialize(target);
}

