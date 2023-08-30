# Unlog

Unlog is a header only library made for Unreal Engine that aims to make logging more effortless and featureful. Some of the features are still experimental are prone to changing. Please leave some feedback on our Issues page. 

##  Feature list

Feature | Status
--- | ---
Define categories at any scope with just one line of code | ✅
Apply a log category based on scope | ✅
Easily write to other output targets (e.g ingame viewport, [Message Log](https://unrealcommunity.wiki/message-log-4wzqj97j)) | ✅
Modern C++ logging syntax with type safety  | ✅
Support for retro-compatible UE_LOG macro syntax using UN_LOG | ✅
Create your own logging targets | ✅
Remove debug strings from the binary when on shipping builds | ✅
Static polymorphism makes sure compiler does most of the work | ✅
Possibility of a few  bugs  | 🐛

## Experimental features
Feature | Status
--- | ---
UNLOG macro function options| 🔨
## 📜 Usage

### Simple category declaration
Unlog makes it easier to declare log categories anywhere in code while still respecting the scope they are declared on.
```cpp
virtual void BeginPlay() override
{
	// Declare log categories at any scope
	UNLOG_CATEGORY( MeaningOfLifeCategory ); 

	// Option A: Using function for maximum type safety
	Unlog::Log< MeaningOfLifeCategory >( "Evaluating the meaning of life." );

	// Option B (Experimental): Log using the custom macro options
	UNLOG( MeaningOfLifeCategory, Log )( "Evaluating the meaning of life" );

	// Option C (Retrocompatible): Use the UE_LOG syntax
	UN_LOG( MeaningOfLifeCategory, Log, "Evaluating the meaning of life ") 
}
```
---
### Make the category implicit
You can also remove the category altogether so Unlog can implicitly pick the category to use. 
```cpp
// Automatically picks the category to use when none is specified
Unlog::Log( "Starting calculation" ); 
```
```cpp
 // Same but using the macro option
UNLOG( Log )( "Starting calculation" );
```
>[!NOTE]
> When omitting the category, it will first try to pick any scoped category that is part of the stack or the default category defined on the [setup](#🔧-setup) 

---
### Scoped category usage
Scoped categories are a powerful way to apply a log category to an entire scope. Meaning all logs without explicit categories will automatically be attributed to the in-scope category.
```cpp
void GoHomeRoutine()
{
	UNLOG_CATEGORY_SCOPED( GoHomeRoutine );
	Unlog::Log( "Character '{0}' starting routine", CharacterName );
	...
	Unlog::Error( "Character '{0}' unable to GoHome due to colliding wall", CharacterName );
}

void Execute()
{
	UNLOG_CATEGORY_SCOPED( RoutineEvaluation );
	Unlog::Log("Starting routine evaluation");

	if( ShouldGoHome() )
	{		
		GoHomeRoutine(); 
	}

	Unlog::Log("Succefuly finished routine evaluation");
}

// Output:
// > RoutineEvaluation: Started routine evaluation
// > GoHomeRoutine: Character 'Gandalf' starting routine
// > GoHomeRoutine: Error: Character 'Gandalf' unable to GoHome due to colliding wall
// > RoutineEvaluation: Succefuly finished routine evaluation
```

## 📀 Compatibility
Unlog has been tested to work from UE 4.26 to UE 5.3. Since the library targets C++14 features, it should teoretically support even older engine versions. It has also been tested on all the major operating systems: Windows, Linux and MacOS with their respective toolchains.

## ☯ What about UE_LOG?
UE_LOG is the tried and tested method of logging in Unreal which is extensively used across thousands of games and projects. It's solid. It also means it's less accepting of changes, some of which could be benefitial to iteration times and usability. Since this project is an attempt to find ways to enhance logging in Unreal, we're willing to sacrifice some of this stability for more features. 

## 🔧 Setup
To use Unlog clone this repository into the Source folder of your project or plugin. After cloning the folder structure should resemble `<ProjectName>/Source/Unlog/Unlog.h`.

Opening `Unlog.h` lets you configure the default logger (named `Unlog`) with the settings more appropriate for your case. 

```cpp
#pragma once
#include <UnlogImplementation.h>

// Example A: Just a simple logger
using Unlog = TUnlog<>;
```
You can also customize the logger with the templated builder pattern:
```cpp
#pragma once
#include <UnlogImplementation.h>

// Example B: Logger with custom output targets and a different default category
UNLOG_CATEGORY( MyLogCategory );
using Unlog = 	TUnlog<>::WithTargets< Target::UELog, Target::Viewport >
			::WithDefaultCategory< MyLogCategory >;
```

> [!IMPORTANT]
> For now it's required to have defined a logger named ```Unlog``` when using the UNLOG macro.

You're also free use this area to declare global categories and other loggers:

```cpp
UNLOG_CATEGORY( GlobalCategory );
using ScreenLogger = TUnlog<>::WithTargets< Target::Viewport >
			     ::WithCategory< GlobalCategory >;
```

> [!NOTE]
> Since Unlog is a header only solution, it doesn't need to incorporated into a specific module to export symbols.


## 💶License and Redistribution

Unlog is totally free for non-commercial purposes and most commercial projects — no licenses need to be purchased for projects with a budget (or revenue) below $250000.

This basically means 99% percent of users won't need to pay anything to use it. Instead we believe that if medium/large developers ever start to extract value out of this library, at their scale, then they're probabaly at a good position to contribute directly towards its development by acquiring a moderatly priced commercial license (which also provides other boons). 

Please reach out to us if you've got any queries: `unlog@guganana.com` 

https://github.com/Guganana/Unlog/blob/122af35c10745499bddd47e0471976b093146da6/LICENSE#L1-L10

## ⤴️ Pull requests
Not taking in pull requests for now. Sorry!

## ✨ More features and information
### Using different verbosity levels
```cpp
// Using functions
Unlog::Log( "Just a log" );
Unlog::Warn( "A warning!" );
Unlog::Error( "An error!!" );
Unlog::Verbose( "A log of dubious value" );
```
```cpp
// Using macros
UNLOG( Log )( "Just a log" );
UNLOG( Warning )( "A warning!" );
UNLOG( Error )( "An error!!" );
UNLOG( Verbose )( "A Log of dubious value" );
```
---
### Conditional logging
```cpp
// Passing a boolean condition as first argument so it only happens when the condition is met
Unlog::Warn( !bIsActive, "Trying to execute operation when component isn't active" );
```
```cpp
// Passing a lambda (returning a bool) makes sure the expression is only evaluated if logging is enabled
Unlog::Warn(  []{ return OnlyCalledWhenLoggingIsActive(); }, "Condition not met" );
```
```cpp
// Macro variant automatically compiles out condition when logging is disabled.
UNCLOG( !bIsActive, Category, Warning )( "Trying to execute operation when component isn't active" );
```
---
### Formatting message and passing in values

#### Numbered formatting
By default Unlog uses the numbered format approach to incorporate values into the message string.
```cpp
Unlog::Log( "Object '{0}' created at {1} with value {2}", 
	     GetNameSafe(MaterialExpression), FDateTime::Now().ToString(), 42 );
//Or
UNLOG(Log)( "Object '{0}' created at {1} with value {2}", 
	     GetNameSafe(MaterialExpression), FDateTime::Now().ToString(), 42 );
// Output:
// > Object 'MaterialExpression_0' created at 2023.08.23-19.58.49 with value 42
```

#### Printf formatting
Or you can always use the old trustable printf by using the "f" suffix function variants:
```cpp
Unlog::Logf( "Object %s created at %s with value %d", 
	     *GetNameSafe(MaterialExpression), *FDateTime::Now().ToString(), 42 );
//Or
UNLOGF(Log)( "Object %s created at %s with value %d", 
	     *GetNameSafe(MaterialExpression), *FDateTime::Now().ToString(), 42 );

// Output:
// > Object 'MaterialExpression_0' created at 2023.08.23-19.58.49 with value 42
```
---
### Using a custom logger
At any point you can create a custom logger to output to other targets:
```cpp
using MyLogger = TUnlog<>
	::WithCategory< MyCategory >
	::WithTargets< Target::TViewport< 10, FColor::Red > >;

MyLogger::Error("Failed to spawn actor!");

// Macro function also accepts a custom logger as the first parameter!
UNLOG( MyLogger, Error )( "Failed to spawn actor!" )
```

---
### Automatic handling of wide char strings

#### When using the logging functions (Experimental)
Unlog bypasses the need to wrap your text in the TEXT() macro by automatically converting `char` string literals into the `TCHAR` set.
```cpp
// Logging in runes like it is 500 A.D.
Unlog::Error("ᚻᛖ ᚳᚹᚫᚦ ᚦᚫᛏ ᚻᛖ ᛒᚢᛞᛖ ᚩᚾ ᚦᚫᛗ ᛚᚪᚾᛞᛖ ᚾᚩᚱᚦᚹᛖᚪᚱᛞᚢᛗ ᚹᛁᚦ ᚦᚪ ᚹᛖᛥᚫ");

// Output:
// > LogGeneral: ᚻᛖ ᚳᚹᚫᚦ ᚦᚫᛏ ᚻᛖ ᛒᚢᛞᛖ ᚩᚾ ᚦᚫᛗ ᛚᚪᚾᛞᛖ ᚾᚩᚱᚦᚹᛖᚪᚱᛞᚢᛗ ᚹᛁᚦ ᚦᚪ ᚹᛖᛥᚫ

// Output without automatic conversion
// > LogGeneral: áš»á›– áš³áš¹áš«áš¦ áš¦áš«á› áš»á›– á›’áš¢á›žá›– áš©áš¾ áš¦áš«á›— á›šášªáš¾á›žá›– áš¾áš©áš±áš¦áš¹á›–ášªáš±á›žáš¢á›— áš¹á›áš¦ áš¦ášª áš¹á›–á›¥áš«
```


>[!NOTE]
>It's important to note that this conversion results in one extra string allocation every time the log expression is hit. If you wish to bypass this allocation completely you should still wrap text with TEXT() macro.

#### When using the logging macro
The UNLOG macro automatically wraps the format text with the TEXT() macro so you won't have to do it. Doing so will result in an compilation error complaining about `'LL': undeclared identifier`. 

---

### Removing log strings from shipping builds
Disabling Unlog will automatically cull out string literals used in logs from the compilation. This helps making the program smaller (even if by just a fraction) and stops potentially sensitive or redundant information to reach the user.

Using the UNLOG macro 100% guarantees the strings are culled. Using the logging functions will almost always result on culling when running at least the minimum level of compiler optimizations (tested on MSVC locally and on gcc using godbolt.com), but it can depend if other functions are being called as part of the logging function arguments.

---

## 👏 Appreciation section

Shoutout to https://github.com/ChristianPanov/lwlog which has been a good source of inspiration for some of the code design patterns.
