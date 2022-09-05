# Codecks.io User Report Plugin for Unreal Engine 5

- Currently only 5.0+ is supported

# Setup

Either clone into your Projects (or Engine) Plugin folder (or any subfolder in it.) for example: `Plugins\CodecksUnreal\`
Or download & extract into such a path.

After installing the plugin should be enabled by default, but make sure it is by going to Edit/Plugins in the Editor and look for CodecksUnreal.
Then go to your project settings and add the generated report token to the settings.

> 💡 If you want to provide a build specific report token you can utilize
> 
> a) BuildGraph with
> 
> ```
> <ModifyConfig File="$(UnrealProjectPath)\Config\DefaultGame.ini" Section="/Script/CodecksUnreal.CodecksSettings" Key="ReportToken" Value="YOUR_NEW_REPORT_TOKEN" />
> ```
> 
> b) Use a shell command `sed` to do so

# Usage

The plugin is designed for both C++ and Blueprint usage. Since a bug report involves UI it is advised to utilize UMG/UserWidgets for it as well.

## Blueprint

First construct a CodecksUserReportRequest in Blueprint and assign values.
All properties can be changed later, and some are optional (like e-mail).

To send the request use the `PostUserReport` node in blueprint.

![](./Docs/PostUserReport.jpg)

To listen for the upload progress directly you can bind to the delegate `` of CodecksUserReportRequest
           
### Further Example

In the project I tested it on the BP looks kind of like that:

![](./Docs/Example_LTC.jpg)

## Directly C++

If for some reason you want to use it purely in Cpp, you can use the [API implementation](./Source/CodecksUnreal/Public/Requests/CodecksUserReportRequest.h) directly.

You can also use the PostUserReport object in Cpp, which will handle most of the stuff behind the scenes.
You can also just use the CodecksUserReportRequest object directly using more of the exposed functionality (or extend it).

```c++
UCodecksUserReportRequest* NewUserReport = NewObject<UCodecksUserReportRequest>();

// Just sending it off and not caring
NewUserReport->CreateReport();

// Sending off and handling updates/errors/completion
NewUserReport->CreateReport([](UCodecksUserReportRequest* Update) {}); 
```

# License

Distributed under the MIT License (MIT) (See accompanying file [LICENSE.md](./LICENSE.txt) (or copy at http://opensource.org/licenses/MIT)
