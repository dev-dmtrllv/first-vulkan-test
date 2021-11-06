#include "Engine.hpp"
#include "Logger.hpp"
#define CHECK_REJECT(subSystem, rejector, msg) if(!subSystem) { rejector(msg); printf(msg); return false; }

namespace NovaEngine
{
	namespace
	{
		v8::Global<v8::Promise::Resolver> configurePromiseResolver_;
		v8::Global<v8::Function> onLoadCallback_;
		v8::Global<v8::Object> configuredValue_;

		std::vector<Engine*> engineInstances_;

		extern "C" void onAbortHandler(int signal_number)
		{
			for (Engine* e : engineInstances_)
				e->terminate();
		}

		SCRIPT_METHOD(onEngineConfigure)
		{
			v8::Isolate* isolate_ = args.GetIsolate();
			v8::HandleScope handle_scope(isolate_);
			v8::Local<v8::Context> context = isolate_->GetCurrentContext();
			v8::Context::Scope context_scope(context);

			if (args.Length() < 1)
			{
				printf("Nein Nein Nein!\n");
			}
			else if (args[0]->IsObject())
			{
				v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(isolate_);
				v8::Local<v8::Promise> p = resolver->GetPromise();
				args.GetReturnValue().Set(p);
				configurePromiseResolver_.Reset(isolate_, resolver);
				configuredValue_.Reset(isolate_, args[0]->ToObject(isolate_));
			}
		}

		SCRIPT_METHOD(log)
		{
			v8::Isolate* isolate = args.GetIsolate();
			printf("\033[;32m[Game]\033[0m ");
			for (int i = 0; i < args.Length(); i++)
			{
				auto a = args[i]->ToString(isolate);
				v8::String::Utf8Value val(a);
				printf("%s", *val);
				if (i != args.Length() - 1)
				{
					printf(", ");
				}
			}
			printf("\n");
		}

		// the game will call this to configure the engine
		SCRIPT_METHOD(onEngineLoad)
		{
			v8::Isolate* isolate_ = args.GetIsolate();
			v8::HandleScope handle_scope(isolate_);
			v8::Local<v8::Context> context = isolate_->GetCurrentContext();
			v8::Context::Scope context_scope(context);

			if (args.Length() < 1)
			{
				printf("Nein Nein Nein!\n");
			}
			else if (args[0]->IsFunction())
			{
				onLoadCallback_.Reset(isolate_, v8::Local<v8::Function>::Cast(args[0]));
			}
			args.GetReturnValue().Set(v8::Undefined(isolate_));
		}

		SCRIPT_METHOD(onEngineStart)
		{
			Engine* engine = ScriptManager::fetchEngineFromArgs(args);
			engine->start();
			printf("on engine start called!\n");
		}

		SCRIPT_METHOD(onShowWindow)
		{
			Engine* engine = ScriptManager::fetchEngineFromArgs(args);
			engine->gameWindow.show();
		}

		static void globalInitializer(ScriptManager* manager, const v8::Local<v8::Object>& o)
		{
			v8::Isolate* isolate = manager->isolate();
			v8::Local<v8::Context> ctx = isolate->GetCurrentContext();

			v8::Local<v8::Object> engineObj = v8::Object::New(isolate);

			engineObj->Set(ctx, manager->createString("onLoad"), manager->createFunction(onEngineLoad));
			engineObj->Set(ctx, manager->createString("log"), manager->createFunction(log));
			engineObj->Set(ctx, manager->createString("start"), manager->createFunction(onEngineStart));

			v8::Local<v8::Object> windowObj = v8::Object::New(isolate);
			windowObj->Set(ctx, manager->createString("show"), manager->createFunction(onShowWindow));
			engineObj->Set(ctx, manager->createString("window"), windowObj);

			o->Set(ctx, manager->createString("Engine"), engineObj);
		};
	};


	std::vector<Terminatable*> Engine::subSystems_;

	char Engine::executablePath_[PATH_MAX];

	Engine::Engine() : AbstractObject(), isRunning_(false), assetManager(this), scriptManager(this), configManager(this), graphicsManager(this), gameWindow(this)
	{
		if (engineInstances_.size() == 0)
			std::signal(SIGABRT, &onAbortHandler);
		engineInstances_.push_back(this);
	}

	bool Engine::initSubSystem(const char* name, SubSystem<>* subSystem)
	{
		Logger* l = Logger::get();

		if (std::count(subSystems_.begin(), subSystems_.end(), subSystem))
		{
			l->error("Subsystem ", name, " is already initialized!");
			return false;
		}
		else
		{
			l->info("Initializing ", name, "...");
			if (!subSystem->initialize())
			{
				l->error("Failed to initialize ", name, "!");
				return false;
			}
			else
			{
				l->info(name, " initialized!");
				subSystems_.push_back(subSystem);
				return true;
			}
		}
	}

	bool Engine::onInitialize(const char* gameStartupScript)
	{
		Engine engine();

		printf("Initializing Engine...\n");

		CHECK(initSubSystem("Asset manager", &assetManager, executablePath()), "Failed to initialize Asset Manager!");
		CHECK(initSubSystem("Script Manager", &scriptManager, globalInitializer), "Failed to initialie Script Manager!");

		scriptManager.load(gameStartupScript == nullptr ? "Game.js" : gameStartupScript);
		if (onLoadCallback_.IsEmpty())
			return false;

		bool configInitialized = false;

		// call Engine.onLoad() to provide a configure callback
		scriptManager.run([&](const ScriptManager::RunInfo& runInfo) {
			v8::Local<v8::Object> recv = v8::Object::New(runInfo.isolate);
			v8::Local<v8::Value> argv[] = { v8::Function::New(runInfo.isolate, onEngineConfigure) };
			onLoadCallback_.Get(runInfo.isolate)->Call(recv, 1, argv)->ToObject(runInfo.isolate);
		});

		auto rejectGameConfig = [&](const char* message) {
			scriptManager.run([&](const ScriptManager::RunInfo& runInfo) {
				v8::Local<v8::String> reason = v8::String::NewFromUtf8(runInfo.isolate, message, v8::NewStringType::kNormal).ToLocalChecked();
				configurePromiseResolver_.Get(runInfo.isolate)->Reject(reason);
			});
		};

		// now we wait till the callback from Engine.onLoad is done running
		// check if the script called the configure function 
		if (!configuredValue_.IsEmpty())
			configInitialized = initSubSystem("Config Manager", &configManager, &configuredValue_);

		CHECK_REJECT(configInitialized, rejectGameConfig, "Could not initialize Config Manager!");

		CHECK_REJECT(gameWindow.create(configManager.getConfig()->name.c_str(), configManager.getConfig()->window), rejectGameConfig, "Could not create Game Window!");

		CHECK_REJECT(initSubSystem("Graphics Manager", &graphicsManager, gameWindow.glfwWindow()), rejectGameConfig, "Could not create graphics stack!");

		return true;
	}

	const char* Engine::executablePath()
	{
		if (executablePath_[0] == '\0')
		{
			int exePathLength = readlink("/proc/self/exe", executablePath_, PATH_MAX);
			if (exePathLength == -1)
			{
				printf("Could not set executablePath_!\n");
			}
			else
			{
				for (size_t i = exePathLength; i > 0; --i)
				{
					if (executablePath_[i] == '/')
					{
						executablePath_[i] = '\0';
						break;
					}
				}
			}
		}
		return executablePath_;
	}

	void Engine::run()
	{
		if (!configurePromiseResolver_.IsEmpty()) // probably first time started (with configuration passed) 
		{
			scriptManager.run([](const ScriptManager::RunInfo& runInfo) {
				configurePromiseResolver_.Get(runInfo.isolate)->Resolve(v8::Local<v8::Value>(v8::Undefined(runInfo.isolate)));
			});
			configurePromiseResolver_.Reset();
		}
	}

	bool Engine::isRunning()
	{
		return isRunning_;
	}

	void Engine::start()
	{
		if (!isRunning_)
		{
			printf("starting engine...\n");

			isRunning_ = true;

			while (gameWindow.isOpen())
			{
				glfwPollEvents();
			}
		}
	}

	void Engine::stop()
	{
		if (isRunning_)
		{
			isRunning_ = false;
		}
	}

	bool Engine::onTerminate()
	{
		if (isRunning_)
			stop();

		configurePromiseResolver_.Reset();
		onLoadCallback_.Reset();
		configuredValue_.Reset();

		configManager.terminate();
		scriptManager.terminate();
		assetManager.terminate();

		Logger::terminate();

		return true;
	}
};