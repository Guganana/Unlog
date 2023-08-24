// Copyright 2023 Guganana. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Developer/MessageLog/Public/MessageLogModule.h>
#include <Developer/MessageLog/Public/IMessageLogListing.h>
#include <DrawDebugHelpers.h>
#include <Modules/ModuleManager.h>
#include <VisualLogger.h>


#define UNLOG_VERSION TEXT("1.0")
#define UNLOG_ENABLED (!UE_BUILD_SHIPPING)

#define UNLOG_COMPILED_OUT  

// TODO FORCEINLINE all possible

// ------------------------------------------------------------------------------------
// Static Generation Helpers
// Templated structs used to select the appropriate template variations when 
// ------------------------------------------------------------------------------------
template< typename InFormatOptions, typename InCategoryPicker, typename InTargetOptions >
struct TStaticConfiguration
{
    using FormatOptions = InFormatOptions;
    using CategoryPicker = InCategoryPicker;
    using TargetOptions = InTargetOptions;
};

template<bool InIsPrintfFormat>
struct TFormatOptions
{
    static constexpr bool IsPrintfFormat = InIsPrintfFormat;
};

// ------------------------------------------------------------------------------------
// Logging Function Generators
// 
// Macros to generate the different variants of logging functions
// ------------------------------------------------------------------------------------

#if UNLOG_ENABLED
#define UNLOG_DECLARE_CATEGORY_LOG_FUNCTION_CONDITIONALS( CategoryPicker, TargetOptions, FunctionName, VerbosityName, IsPrintf ) \
    template<typename TCategory = typename CategoryPicker, typename FMT, typename... ArgTypes> \
    FORCEINLINE static void FunctionName(const FMT& Format, ArgTypes... Args)\
    {\
        using Configuration = TStaticConfiguration< TFormatOptions<IsPrintf>, CategoryPicker, TargetOptions >;\
        Unlogger::Get().UnlogPrivateImpl< Configuration >(Format, ELogVerbosity::VerbosityName, Args...);\
    }\
    template<typename TCategory = typename CategoryPicker, typename FMT, typename... ArgTypes>\
    FORCEINLINE static void FunctionName(const bool Condition, const FMT& Format, ArgTypes... Args)\
    {\
        if(Condition)\
        {\
            using Configuration = TStaticConfiguration< TFormatOptions<IsPrintf>, CategoryPicker, TargetOptions >;\
            Unlogger::Get().UnlogPrivateImpl< Configuration >(Format, ELogVerbosity::VerbosityName, Args...);\
        }\
    }\
    template<typename TCategory = typename CategoryPicker, typename FMT, typename... ArgTypes>\
    FORCEINLINE static void FunctionName(const TFunction<bool()>& LambdaCondition, const FMT& Format, ArgTypes... Args)\
    {\
        FunctionName( LambdaCondition(), Format, Args... );\
    }
#else
#define UNLOG_DECLARE_CATEGORY_LOG_FUNCTION_CONDITIONALS( CategoryPicker, TargetOptions, FunctionName, VerbosityName, IsPrintf ) \
    template< typename... TAllArgTypes > \
    FORCEINLINE static void FunctionName(TAllArgTypes... Args){}
#endif // UNLOG_ENABLED

#define UNLOG_DECLARE_CATEGORY_LOG_FUNCTION( CategoryPicker, TargetOptions, FunctionName, VerbosityName )\
    UNLOG_DECLARE_CATEGORY_LOG_FUNCTION_CONDITIONALS( CategoryPicker, TargetOptions, FunctionName, VerbosityName, false )\
    UNLOG_DECLARE_CATEGORY_LOG_FUNCTION_CONDITIONALS( CategoryPicker, TargetOptions, FunctionName##f, VerbosityName, true )

// ------------------------------------------------------------------------------------
// Categories
// 
// Declaring a category generates its own class with a unique and just one statically 
// constructed instance. 
// 
// Category declarations respect the scope they are declared on and are lazyly 
// constructed when used for the first time by a log function. Instace objects will 
// live for rest of app's execution.
// ------------------------------------------------------------------------------------

class UnlogCategoryBase
{
private:
    FName CategoryName;
    ELogVerbosity::Type Verbosity;

public:

    UnlogCategoryBase(const FName& InName, ELogVerbosity::Type InVerbosity)
        : CategoryName(InName)
        , Verbosity(InVerbosity)
    {}

    const FName& GetName() const
    {
        return CategoryName;
    }

    ELogVerbosity::Type GetVerbosity() const
    {
        return Verbosity;
    }
};

template<typename TCategory>
class UnlogCategoryCRTP : public UnlogCategoryBase
{

public:
    using UnlogCategoryBase::UnlogCategoryBase;

    static TCategory& Static()
    {
        static TCategory Type = TCategory::Construct();
        return Type;
    }

    static void PickCategory( UnlogCategoryBase*& InOutCategory )
    {
        InOutCategory = &Static();
    }
};

template< typename TCategory >
struct FUnlogScopedCategory
{
    FUnlogScopedCategory()
    {
        Unlogger::Get().PushCategory(TCategory::Static());
    }

    ~FUnlogScopedCategory()
    {
        Unlogger::Get().PopCategory();
    }
};

/**
* Creates a new Unlog category.
* Can be used anywhere (headers, inside .cpp files, inside classes or functions)
* Generates a unique class for this category and can be shared with other files if put in a common header
*/
#define UNLOG_CATEGORY(CategoryName)  \
class CategoryName : public UnlogCategoryCRTP< CategoryName > \
{\
protected:\
    using UnlogCategoryCRTP<CategoryName>::UnlogCategoryCRTP; \
    static CategoryName Construct()\
    {\
        return CategoryName( TEXT( #CategoryName ), ELogVerbosity::Log );\
    }\
friend class UnlogCategoryCRTP< CategoryName >;\
};

#define UNLOG_CATEGORY_PUSH( CategoryName ) \
    FUnlogScopedCategory<CategoryName> ScopedCategory_##CategoryName = FUnlogScopedCategory<CategoryName>();

#define UNLOG_CATEGORY_SCOPED(CategoryName) \
    UNLOG_CATEGORY(CategoryName) \
    UNLOG_CATEGORY_PUSH(CategoryName)


// Create the default category used by Unlog
UNLOG_CATEGORY(LogGeneral)



// ------------------------------------------------------------------------------------
// Contexts (Experimental)
// 
// Tracks when the program has entered a specific "context", able to be queried 
// by another system later down the callstack to selectively exclude certain log calls
// without creating a direct dependency between systems to query state.
// 
// e.g. Ignore logging a widget when part of the editor callstack
// 
// Could technically be repurposed to selectively run other code besides logging.
// ------------------------------------------------------------------------------------

template <typename ActualType>
class UnlogContextBase
{
public:

    UnlogContextBase(const FName& InName)
        : ContextName(InName)
        , Counter(0u)
    {}

    FORCEINLINE static ActualType& Static()
    {
        static ActualType Type = ActualType::Construct();
        return Type;
    }

    FORCEINLINE const FName& GetName() const
    {
        return ContextName;
    }

    FORCEINLINE void IncrementCounter()
    {
        Counter++;
    }

    FORCEINLINE void DecrementCounter()
    {
        check(Counter > 0u);
        Counter--;
    }

    FORCEINLINE bool IsActive() const
    {
        return Counter > 0u;
    }

    template< typename Functor >
    FORCEINLINE static void WhenActive(Functor Func)
    {
        if (ActualType::Static().IsActive())
        {
            Func();
        }
    }

    template< typename Functor >
    FORCEINLINE static void WhenNotActive(Functor Func)
    {
        if (!ActualType::Static().IsActive())
        {
            Func();
        }
    }

private:
    FName ContextName;
    uint32 Counter;
};

template <typename ActualType>
class UnloggerScopedContext
{
private:
    bool Value;
public:
    UnloggerScopedContext(bool InValue)
        : Value(InValue)
    {
        if (Value)
        {
            ActualType::Static().IncrementCounter();
        }
    }

    ~UnloggerScopedContext()
    {
        if (Value)
        {
            ActualType::Static().DecrementCounter();
        }
    }

};


#define UNLOG_CONTEXT(ContextName) \
class ContextName : public UnlogContextBase< ContextName > \
{\
public:\
    using UnlogContextBase< ContextName >::UnlogContextBase; \
    static ContextName Construct()\
    {\
        return ContextName( TEXT( #ContextName ) );\
    }\
};

#define scopedcontext(ContextName, ContextValue) \
UnloggerScopedContext< ContextName > ScopedContext_##ContextName( ContextValue )

// ------------------------------------------------------------------------------------
// Runtime Settings and Targets (Experimental)
// ------------------------------------------------------------------------------------

class UnlogRuntimeTargetBase
{
public:
    virtual ~UnlogRuntimeTargetBase() {}
    virtual void ProcessLog(const FName& Category, ELogVerbosity::Type Verbosity, const FString& Message) = 0;
};

struct UnlogRuntimeSettingsBase
{
public:

    UnlogRuntimeSettingsBase()
        : Targets()
        , DefaultCategory()
    {
    }

    template< typename TTarget>
    TSharedRef<TTarget> AddTarget()
    {
        auto Target = TSharedRef<UnlogRuntimeTargetBase>(MakeShareable(new TTarget()));
        Targets.Add(Target);
        return StaticCastSharedRef<TTarget>(Target);
    }

    const TArray<TSharedRef<UnlogRuntimeTargetBase>>& GetTargets()
    {
        return Targets;
    }

    template< typename TCategory >
    void SetDefaultCategory()
    {
        DefaultCategory = &TCategory::Static();
    }

    UnlogCategoryBase& GetDefaultCategory()
    {
        return DefaultCategory ? *DefaultCategory : LogGeneral::Static();
    }

private:
    TArray<TSharedRef<UnlogRuntimeTargetBase>> Targets;
    UnlogCategoryBase* DefaultCategory;
};

template< typename ConcreteSettings >
struct RuntimeSettingsCRTP : public UnlogRuntimeSettingsBase
{
public:
    using UnlogRuntimeSettingsBase::UnlogRuntimeSettingsBase;

    static ConcreteSettings MakeSettings()
    {
        ConcreteSettings Settings;
        Settings.PopulateSettings();
        return Settings;
    }

    static ConcreteSettings& Static()
    {
        static ConcreteSettings Type = MakeSettings();
        return Type;
    }
};

struct UnlogDefaultRuntimeSettings : public RuntimeSettingsCRTP<UnlogDefaultRuntimeSettings>
{
public:
    void PopulateSettings()
    {
        SetDefaultCategory<LogGeneral>();
    }

    using DefaultSettings = LogGeneral;
};

template< typename TUnlogSettings >
struct SetGlobalUnlogSettings
{
    const static int32 Initializer;
};

#if UNLOG_ENABLED
#define UNLOG_DEFAULT_SETTINGS( SettingsName ) \
const int32 SetGlobalUnlogSettings< SettingsName >::Initializer = [] { Unlogger::Get().ApplySettings< SettingsName >();  return 0; }();
#else
#define UNLOG_DEFAULT_SETTINGS( SettingsName ) UNLOG_COMPILED_OUT
#endif

// ------------------------------------------------------------------------------------
// Telemetry
// 
// Sending a single telemetry event on the first time the logger is used, we try to
// include as little data as possible while still giving us valuable information to
// gauge the usage and version distribution of Unlog. This helps us make better and 
// morinformed decisions on how to improve.
// ------------------------------------------------------------------------------------
#if WITH_EDITOR && UNLOG_ENABLED
class FTelemetryDispatcher
{

public:
    FTelemetryDispatcher()
    {
        const auto Prod = TEXT("Unlog");
        const auto AppId = AppId::Get().ToString();
        const auto SHA = TEXT("None");
        const auto Date = FDateTime(2023, 8, 20).ToIso8601();
        const auto UEVersion = FEngineVersion::Current().ToString();

        // Running request as a console command cuts direct dependencies to the
        // http module which would make Unlog harder to include since it's a header only lib
        const FString Cmd = FString::Format(
            TEXT(
                "http TEST \"1\" "
                "\"https://api.guganana.com/api/usage?data="
                "%7B"
                "%22pluginName%22%3A%22{0}%22"
                "%2C%22appId%22%3A%22{1}%22"
                "%2C%22versionFriendly%22%3A%22{2}%22"
                "%2C%22versionSHA%22%3A%22{3}%22"
                "%2C%22versionDate%22%3A%22{4}%22"
                "%2C%22unrealVersion%22%3A%22{5}%22"
                "%7D\""
            ),
            { Prod, AppId, UNLOG_VERSION, SHA, Date, UEVersion }
        );

        const auto RunCmd = [Cmd] {
            GEngine->Exec(nullptr, *Cmd);
        };

        if (!GEngine || !GEngine->IsInitialized())
        {
            FCoreDelegates::OnPostEngineInit.AddLambda(RunCmd);
        }
        else
        {
            RunCmd();
        }
    }

private:
    struct AppId
    {
        static FGuid Get()
        {
            static FGuid Id = FGuid();

            if (Id == FGuid())
            {
                if (!TryReadFromFile(Id))
                {
                    Id = FGuid::NewGuid();
                    SaveGuid(Id);
                }
            }

            return Id;
        }

        static FString GetIdFilePath()
        {
            return FPaths::Combine(FPaths::EngineVersionAgnosticUserDir(), TEXT("Unlog"), TEXT("Id"));
        }

        static bool TryReadFromFile(FGuid& OutGuid)
        {
            FString FileData;
            if (FFileHelper::LoadFileToString(FileData, *GetIdFilePath()))
            {
                FGuid Result;
                if (FGuid::Parse(FileData, Result))
                {
                    OutGuid = Result;
                    return true;
                }
            }

            return false;
        }

        static void SaveGuid(const FGuid& Guid)
        {
            FFileHelper::SaveStringToFile(Guid.ToString(), *GetIdFilePath());
        }
    };
};
#endif // WITH_EDITOR && UNLOG_ENABLED

// ------------------------------------------------------------------------------------
// Unlog internals
// ------------------------------------------------------------------------------------

#if UNLOG_ENABLED
class Unlogger
{
private:
    // Settings should never be destroyed since they are statically created
    UnlogRuntimeSettingsBase* Settings;

    // Pushed categories temporarily override the default category, usually during a certain scope
    TArray<UnlogCategoryBase*> PushedCategories;

public:

    static Unlogger& Get()
    {
        static Unlogger Logger = CreateLogger();
        return Logger;
    }

    static Unlogger CreateLogger()
    {
        // Start with default settings
        Unlogger Logger;
        Logger.ApplyRuntimeSettingsInternal<UnlogDefaultRuntimeSettings>();

#if WITH_EDITOR
        static const FTelemetryDispatcher TelemetryDispatcher = FTelemetryDispatcher();
#endif
        return Logger;
    }

    template< typename TSettings > 
    static void ApplyRuntimeSettings()
    {
        Unlogger::Get().ApplyRuntimeSettingsInternal<TSettings>();
    }

    template< typename TSettings >
    void ApplyRuntimeSettingsInternal()
    {
        Settings = &TSettings::Static();
    }

    void PushCategory(UnlogCategoryBase& Category)
    {
        PushedCategories.Push(&Category);
    }

    void PopCategory()
    {
        check(PushedCategories.Num() > 0);
        PushedCategories.Pop();
    }

    FORCEINLINE const TCHAR* ResolveString(const TCHAR* Format)
    {
        return Format;
    }

    FORCEINLINE const TCHAR* ResolveString(const char* Format)
    {
        return UTF8_TO_TCHAR(Format);
    }


    template<typename CategoryPicker>
    FORCEINLINE const UnlogCategoryBase& PickCategory()
    {
        UnlogCategoryBase* SelectedCategory = PushedCategories.Num() > 0 ? PushedCategories.Last() : nullptr;

        CategoryPicker::PickCategory(SelectedCategory);

        check(SelectedCategory);
        return *SelectedCategory;
    }

    // Use ordered arguments format
    template< 
        typename FormatOptions,
        typename FMT,
        typename... ArgTypes >
    FORCEINLINE typename TEnableIf<!FormatOptions::IsPrintfFormat, FString>::Type  ProcessFormatString(const FMT& Format, ArgTypes... Args)
    {
        static_assert(TAnd<TIsConstructible<FStringFormatArg, ArgTypes>...>::Value, "Invalid argument type passed to UnlogPrivateImpl");
        return FString::Format(ResolveString(Format), FStringFormatOrderedArguments({ Args... }));
    }

    // Use Printf format
    template<
        typename FormatOptions,
        typename FMT,
        typename... ArgTypes >
    FORCEINLINE typename TEnableIf<FormatOptions::IsPrintfFormat, FString>::Type ProcessFormatString(const FMT& Format, ArgTypes... Args)
    {
        static_assert(!TIsArrayOrRefOfType<FMT, char>::Value, "Unlog's printf style functions only support text wrapped by TEXT()");
        FString Result = FString::Printf(Format, Args...);
        return Result;
    }   

    template<typename StaticConfiguration, typename FMT, typename... ArgTypes>
    void UnlogPrivateImpl(const FMT& Format, ELogVerbosity::Type Verbosity, ArgTypes... Args)
    {
        const auto& Category = PickCategory<StaticConfiguration::CategoryPicker>();
        FName CategoryName = Category.GetName();

        if (Verbosity <= Category.GetVerbosity() && Verbosity != ELogVerbosity::NoLogging )
        {
            FString Result = ProcessFormatString<StaticConfiguration::FormatOptions>(Format, Args...);
        
            // Execute all static targets
            StaticConfiguration::TargetOptions::Call(Category, Verbosity, Result);
        }
    }
};
#endif // UNLOG_ENABLED

// ------------------------------------------------------------------------------------
// Targets
// 
// Specifies where log messages should be routed to.
// 
// By default all messages are routed to FMsg::Log so it's as similar as executing 
// UE_LOG. There's also a few other options like the GameScreen or the MessageLog.
// 
// Multiple targets can be used by chaining them inside a TMultiTarget
// ------------------------------------------------------------------------------------
namespace Target
{
    /**
    * Combines multiple targets together.
    * 
    * e.g TMultiTarget< Target::UELog, Target::GameScreen>
    */
    template< typename... TTargets >
    struct TMultiTarget
    {
        static void Call(const UnlogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FString& Message)
        {
            auto Ignore = { (TTargets::Call(Category, Verbosity, Message),0)... };
        }
    };

    struct UELog
    {
        static void Call(const UnlogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FString& Message)
        {
            FMsg::Logf(nullptr, 0, Category.GetName(), Verbosity, TEXT("%s"), *Message);
        }
    };
    
    struct MessageLog
    {
        static TSharedRef<IMessageLogListing> GetLogListing(FMessageLogModule& MessageLogModule, const FName& CategoryName)
        {
            auto Listing = MessageLogModule.GetLogListing(CategoryName);
            Listing->SetLabel(FText::FromString(CategoryName.ToString()));
            return Listing;
        }

        static EMessageSeverity::Type VerbosityToSeverity(ELogVerbosity::Type Verbosity)
        {
            switch (Verbosity)
            {
            case ELogVerbosity::Error:
                return EMessageSeverity::Error;
            case ELogVerbosity::Warning:
                return EMessageSeverity::Warning;
            }
            return EMessageSeverity::Info;
        }

        static void Call(const UnlogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FString& Message)
        {
            FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");

            auto LogListing = GetLogListing(MessageLogModule, Category.GetName());
            LogListing->AddMessage(
                FTokenizedMessage::Create(
                    VerbosityToSeverity(Verbosity),
                    FText::FromString(Message)
                )
            );

            if (Verbosity == ELogVerbosity::Error)
            {
                MessageLogModule.OpenMessageLog(Category.GetName());
            }
        }
    };


    template< int TimeOnScreen, const FColor& InColor >
    struct TGameScreen
    {
        static void Call(const UnlogCategoryBase& Category, ELogVerbosity::Type Verbosity, const FString& Message)
        {
            GEngine->AddOnScreenDebugMessage(INDEX_NONE, TimeOnScreen, InColor, Message);
        }
    };
    using GameScreen = TGameScreen<3, FColor::Cyan>;

    using Default = UELog;    
}

template< typename TCategory >
struct TSpecificCategory
{
    static UnlogCategoryBase* PickCategory( UnlogCategoryBase*& InOutCategory )
    {
        InOutCategory = &TCategory::Static();
    }
};

template< typename TDefaultCategory = typename LogGeneral >
struct TDeriveCategory
{
    static UnlogCategoryBase* PickCategory( UnlogCategoryBase*& InOutCategory )
    {
        // Only populate with default category if it's not already derived
        if (InOutCategory == nullptr)
        {
            InOutCategory = &TDefaultCategory::Static();
        }
    }
};

// ------------------------------------------------------------------------------------
// Unlog library
// ------------------------------------------------------------------------------------

template<typename InTargetOptions = Target::Default , typename InCategoryPicker = TDeriveCategory<> >
struct TUnlog
{
    using CategoryPicker = InCategoryPicker;
    using TargetOptions = InTargetOptions;

    template< typename... Targets >
    using WithTargets = TUnlog< Target::TMultiTarget<Targets...>, InCategoryPicker >;

    template< typename... Targets >
    using AddTarget = TUnlog< Target::TMultiTarget<InTargetOptions, Targets...>, InCategoryPicker >;

    template< typename InCategory >
    using WithDefaultCategory = TUnlog< InTargetOptions, TDeriveCategory<InCategory> >;

    template< typename InCategory >
    using WithCategory = TUnlog< InTargetOptions, TSpecificCategory<InCategory> >;

    UNLOG_DECLARE_CATEGORY_LOG_FUNCTION(InCategoryPicker, TargetOptions, Log, Log)
    UNLOG_DECLARE_CATEGORY_LOG_FUNCTION(InCategoryPicker, TargetOptions, Warn, Warning)
    UNLOG_DECLARE_CATEGORY_LOG_FUNCTION(InCategoryPicker, TargetOptions, Error, Error)
    UNLOG_DECLARE_CATEGORY_LOG_FUNCTION(InCategoryPicker, TargetOptions, Display, Display)
    UNLOG_DECLARE_CATEGORY_LOG_FUNCTION(InCategoryPicker, TargetOptions, Verbose, Verbose)
    UNLOG_DECLARE_CATEGORY_LOG_FUNCTION(InCategoryPicker, TargetOptions, VeryVerbose, VeryVerbose)

    template< typename TVisualizer, typename... TArgs>
    static void Debug( UObject* Owner, /*FName Name, */ TArgs... Args)
    {
    #if UNLOG_ENABLED
        TVisualizer::Display(Owner, Args...);
    #endif // UNLOG_ENABLED
    }

};


// ------------------------------------------------------------------------------------
//  Unlog macros 
// ------------------------------------------------------------------------------------

template<typename MacroOptions, bool IsPrintfFormat, typename FMT, typename... TArgs>
FORCEINLINE void UnlogMacroExpand(ELogVerbosity::Type InVerbosity, const FMT& Format, TArgs... Args)
{
    using Configuration = TStaticConfiguration< 
        TFormatOptions<IsPrintfFormat>, 
        MacroOptions::UnlogOptions::CategoryPicker,
        MacroOptions::UnlogOptions::TargetOptions
    >;

    Unlogger::Get().UnlogPrivateImpl<Configuration>(Format, InVerbosity, Args...);
}


template<typename InMacroArgs, typename InTUnlog >
struct TMacroOptions
{
    using UnlogOptions = typename InMacroArgs::template GetSettings<InTUnlog>;
};


template<typename... TArgs>
struct TMacroArgs;

template<template <typename, typename> class TUnlogSettings, typename TargetOptions, typename CategoryPicker >
struct TMacroArgs< TUnlogSettings< TargetOptions, CategoryPicker> >
{
    template< typename TSourceSettings >
    using GetSettings = typename TUnlog< TargetOptions, CategoryPicker>;
};

//template<template <typename> class TOuter, typename TCategory>
//struct TMacroArgs<TOuter<TCategory>>
//{
//    template< typename TSourceSettings >
//    using GetSettings = typename TUnlog< typename TSourceSettings::TargetOptions, TSpecificCategory<TOuter<TCategory>> >;
//};

template<typename TCategory>
struct TMacroArgs<TCategory>
{
    template< typename TSourceSettings >
    using GetSettings = typename TUnlog< typename TSourceSettings::TargetOptions, TSpecificCategory<TCategory> >;
};

template<>
struct TMacroArgs<>
{
    template< typename TSourceSettings >
    using GetSettings = TSourceSettings; //typename TUnlog< typename TSourceSettings::TargetOptions, TSpecificCategory<TCategory> >;
};




//template< typename TCategory, typename TUnlogSettings = Unlog >
//struct TMacroOptions
//{
//    using UnlogOptions = TUnlog< typename TUnlogSettings::TargetOptions, TSpecificCategory< TCategory > >;
//};
//
//template< typename... TSettingsArgs >
//struct TMacroOptions< TDeriveCategory<>, TUnlog<TSettingsArgs...> >
//{
//    using UnlogOptions = TUnlog<TSettingsArgs...>;
//};

////
////template<typename InTargetOptions, typename InCategoryPicker >
////struct TMacroOptions<TUnlog<InTargetOptions, InCategoryPicker>>
////{
////    using TargetOptions = InTargetOptions;
////    using CategoryOptions = InCategoryPicker;
////};
////
////template<typename TCategory >
////struct TMacroOptions<TCategory>
////{
////    using TargetOptions = Target::Default;
////    using CategoryOptions = TSpecificCategory<TCategory>;
////};
//
//template<>
//struct TMacroOptions< TDeriveCategory<>, Unlog>
//{
//    using UnlogOptions = Unlog<>;
//};

#define EXPAND( ... ) __VA_ARGS__

#if UNLOG_ENABLED
#define UNLOG( InMacroArgs, VerbosityName, Message, ... ) \
    UnlogMacroExpand< TMacroOptions< TMacroArgs< InMacroArgs >, Unlog >, false >( ELogVerbosity::VerbosityName, TEXT( Message ), ##__VA_ARGS__);

#define UNLOGF( InMacroArgs, VerbosityName, Message, ... ) \
    UnlogMacroExpand< TMacroOptions< TMacroArgs< InMacroArgs >, Unlog >, true >( ELogVerbosity::VerbosityName, TEXT( Message ), ##__VA_ARGS__);

#define UNCLOG( Condition, InMacroArgs, VerbosityName, Message, ... ) \
    if( Condition ) \
    {\
        UnlogMacroExpand< TMacroOptions< TMacroArgs< InMacroArgs >, Unlog >, false >( ELogVerbosity::VerbosityName, TEXT( Message ), ##__VA_ARGS__); \
    }
#define UNCLOGF( Condition, InMacroArgs, VerbosityName, Message, ... ) \
    if( Condition ) \
    {\
        UnlogMacroExpand< TMacroOptions< TMacroArgs< InMacroArgs >, Unlog >, true >( ELogVerbosity::VerbosityName, TEXT( Message ), ##__VA_ARGS__); \
    }
#else
#define UNLOG(...) UNLOG_COMPILED_OUT
#define UNLOGF(...) UNLOG_COMPILED_OUT
#define UNCLOG(...) UNLOG_COMPILED_OUT
#define UNCLOGF(...) UNLOG_COMPILED_OUT
#endif // UNLOG_ENABLED

// ------------------------------------------------------------------------------------
// Debug Visualization ( Early prototype )
// ------------------------------------------------------------------------------------

namespace Viz
{
    // TODO check for shipping since drawdebug isn't available on shipping
    // #define UNDEBUG( Type, ... ) Unlog::Debug< Viz::Type >( this, __VA_ARGS__ )

    FORCEINLINE void DrawLabel(UWorld* World, const FVector& Position, const FString& Label, const FString& Value)
    {
        auto CategoryName = LogGeneral::Static().GetName();

        const FString& Message = FString::Format(
            TEXT("| Category: {0}\n")
            TEXT("| Value: {1}\n")
            TEXT("| {2}"),
            { CategoryName.ToString(), Value, Label }
        );

        DrawDebugString(World, Position, Message, nullptr, FColor::White, -1.000000, false, 1.f);
    }

    struct Location
    {
        FORCEINLINE static void Display(UObject* Owner, const FVector& Position, const FColor& Color = FColor::Red, float Radius = 10.f)
        {
            DrawDebugSphere(Owner->GetWorld(), Position, Radius, 12, Color, false, -1.f, SDPG_World, 2.f);
            DrawLabel(Owner->GetWorld(), Position, TEXT("Test position"), Position.ToString());
            FVisualLogger::GeometryShapeLogf(Owner, "Test", ELogVerbosity::Log, Position, Radius, Color, TEXT(""));//, ##__VA_ARGS__)
        }
    };

    struct Direction
    {
        FORCEINLINE static void Display(UObject* Owner, const FVector& Start, const FVector& Direction, const FColor& Color = FColor::Cyan)
        {
            DrawDebugDirectionalArrow(Owner->GetWorld(), Start, Start + (Direction.GetSafeNormal() * 100.f), 10.f, Color, true, -1.f, SDPG_World, 2.f);
            DrawLabel(Owner->GetWorld(), Direction, TEXT("Test direction"), Direction.ToString());
            FVisualLogger::ArrowLogf(Owner, "Test", ELogVerbosity::Log, Start, Start + Direction, Color, TEXT(""));//, ##__VA_ARGS__)
        }
    };
};


// ------------------------------------------------------------------------------------
// Testing
// ------------------------------------------------------------------------------------
namespace UnlogTesting
{
    // Simple test to ensure everything compiles correctly; outputs not tested
    void CompileTest()
    {
        UNLOG_CATEGORY(TestCategory);

        using Unlog = TUnlog<>;
               
        // Specific category
        UNLOG(TestCategory, Log, "A");
        UNLOG(TestCategory, Warning, "B");
        UNLOG(TestCategory, Error, "C");
        UNLOG(TestCategory, Verbose, "D");

        Unlog::Log<TestCategory>("A");
        Unlog::Warn<TestCategory>("B");
        Unlog::Error<TestCategory>("C");
        Unlog::Verbose<TestCategory>("D");

        // Derive category
        UNLOG(, Log, "A");
        Unlog::Log("A");

        // Format - numbered args
        const FString ExampleString( TEXT("Hey") );
        const int32 ExampleInt = 42;
        Unlog::Log("{0}: {1}", ExampleString, ExampleInt);
        UNLOG(,Log,"{0}: {1}", ExampleString, ExampleInt);

        // Format - printf
        Unlog::Logf(TEXT("%s: %d"), *ExampleString, ExampleInt);
        UNLOGF(,Log, "%s: %d", *ExampleString, ExampleInt);

        // Using custom logger
        using CustomUnlog = TUnlog<>::WithCategory< TestCategory >::WithTargets< Target::TGameScreen< 10, FColor::Yellow > >;
        CustomUnlog::Error("X");
        UNLOG(CustomUnlog, Error, "X");

        // Contional logging
        const bool Value = false;
        Unlog::Warn(Value, "Y");
        //Unlog::Warn([]{return false;}, "Y");
        Unlog::Warnf(Value, TEXT("Y"));
        //Unlog::Warnf([] {return false; }, TEXT("Y"));
        UNCLOG(Value, ,Warning, "Y");
        UNCLOGF(Value, , Warning, "Y");
    }
}
