#ifndef ENGINE_SCRIPT_MANAGER_HPP
#define ENGINE_SCRIPT_MANAGER_HPP

#include "framework.hpp"
#include "SubSystem.hpp"

#define SCRIPT_METHOD(name) static void name(const v8::FunctionCallbackInfo<v8::Value>& args)


namespace NovaEngine
{
	class ScriptManager;

	typedef void(*ScriptManagerGlobalInitializer)(ScriptManager* manager, const v8::Local<v8::Object>&);

	class ScriptManager : public SubSystem<ScriptManagerGlobalInitializer>
	{
	public:
		struct RunInfo
		{
			RunInfo(ScriptManager* m, v8::Isolate* i, v8::Local<v8::Context>& c) : scriptManager(m), isolate(i), context(c) {}
			ScriptManager* scriptManager;
			v8::Isolate* isolate;
			v8::Local<v8::Context>& context;
		};

		template<typename Callback>
		static void iterateObjectKeys(v8::Local<v8::Object> obj, Callback callback)
		{
			using namespace v8;

			Local<Context> ctx = obj->CreationContext();
			Isolate* isolate = ctx->GetIsolate();

			Local<Array> keys = obj->GetOwnPropertyNames(ctx, PropertyFilter::ALL_PROPERTIES).ToLocalChecked();

			for (uint32_t i = 0, l = keys->Length(); i < l; i++)
			{
				Local<String> key = keys->Get(ctx, i).ToLocalChecked()->ToString(ctx).ToLocalChecked();
				String::Utf8Value keyVal(key);
				Local<Value> val = obj->Get(ctx, key).ToLocalChecked();
				callback(*keyVal, val);
			}
		}

		static Engine* fetchEngineFromArgs(const v8::FunctionCallbackInfo<v8::Value>& args);

		static void printObject(v8::Isolate* isolate, const v8::Local<v8::Value>& o, const char* name = nullptr);

	private:
		ENGINE_SUB_SYSTEM_CTOR(ScriptManager),
			createParams_(),
			isolate_(nullptr),
			context_(),
			scriptManagerReference_(),
			modules_(),
			moduleRequireCounter_(0)
		{}

		static void onRequire(const v8::FunctionCallbackInfo<v8::Value>& args);
		static ScriptManager* callbackDataToScriptManager(const v8::Local<v8::Context>& ctx, const v8::Local<v8::Value>&);

		static std::unique_ptr<v8::Platform> platform_;

		v8::Isolate::CreateParams createParams_;
		v8::Isolate* isolate_;
		v8::Global<v8::Context> context_;
		v8::Global<v8::Number> scriptManagerReference_;
		std::unordered_map<std::string, v8::Global<v8::Object>> modules_;
		size_t moduleRequireCounter_;

		std::string getRelativePath(const std::string& str);

	protected:
		static std::vector<ScriptManager*> instances_;

		bool runScript(v8::Local<v8::Context> context, const char* scriptString);

		bool onInitialize(ScriptManagerGlobalInitializer globalInitializer);
		bool onTerminate();

	public:
		v8::Local<v8::String> createString(const char* string);
		v8::Local<v8::Number> createNumber(uint32_t num);
		v8::Local<v8::Number> createNumber(int32_t num);
		v8::Local<v8::Boolean> createBool(bool boolean);
		v8::Local<v8::Array> createArray();
		v8::Local<v8::Object> createObject();
		v8::Local<v8::Function> createFunction(v8::FunctionCallback);

		v8::Isolate* isolate();
		v8::Local<v8::Context> context();

		void load(const char* path, bool isJsonModule = false);

		template<typename RunCallback>
		void run(RunCallback callback)
		{
			v8::Locker isolateLocker(isolate_);
			v8::Isolate::Scope isolate_scope(isolate_);

			v8::HandleScope handleScope(isolate_);
			v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate_, context_);
			v8::Context::Scope contextScope(context);

			RunInfo runInfo(this, isolate_, context);
			callback(runInfo);
		}


	private:
		void handleRequire(const v8::FunctionCallbackInfo<v8::Value>& args);
	};


}

#endif
