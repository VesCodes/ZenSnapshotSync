#include "ZenSnapshotSyncToolbar.h"

#include <ToolMenus.h>
#include <Interfaces/ITargetPlatform.h>
#include <Interfaces/ITargetPlatformManagerModule.h>
#include <Misc/AsyncTaskNotification.h>
#include <Styling/AppStyle.h>

#include "ZenSnapshotSyncModule.h"

#define LOCTEXT_NAMESPACE "ZenSnapshotSync"

FZenSnapshotSyncToolbar::FZenSnapshotSyncToolbar()
{
	SnapshotSyncModule = FModuleManager::LoadModulePtr<FZenSnapshotSyncModule>(UE_MODULE_NAME);
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &ThisClass::RegisterMenus));
}

FZenSnapshotSyncToolbar::~FZenSnapshotSyncToolbar()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	CancelSnapshotSyncTasks();
}

void FZenSnapshotSyncToolbar::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
	FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("ZenSnapshotSyncToolbar");

	FUIAction SnapshotSyncMenuAction;
	SnapshotSyncMenuAction.IsActionVisibleDelegate = FIsActionButtonVisible::CreateRaw(SnapshotSyncModule, &FZenSnapshotSyncModule::CanQuerySnapshots);

	FToolMenuEntry& SnapshotSyncMenuButton = Section.AddEntry(FToolMenuEntry::InitComboButton(
		"SnapshotSyncMenu",
		SnapshotSyncMenuAction,
		FNewToolMenuDelegate::CreateRaw(this, &ThisClass::MakeSnapshotSyncMenu),
		LOCTEXT("SnapshotSyncMenuTitle", "Sync Snapshots"),
		FText::GetEmpty(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "FontEditor.Update")
	));

	SnapshotSyncMenuButton.StyleNameOverride = "CalloutToolbar";
}

void FZenSnapshotSyncToolbar::MakeSnapshotSyncMenu(UToolMenu* Menu)
{
	FToolMenuSection& Section = Menu->AddSection("Snapshots");

	// Fetch latest descriptors only when there are no active sync tasks
	if (SnapshotSyncTasks.IsEmpty())
	{
		LatestSnapshotDescriptors.Reset();
		SnapshotSyncModule->QuerySnapshots(LatestSnapshotDescriptors);
	}

	ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();

	TMap<ITargetPlatform*, TArray<const FZenSnapshotDescriptor*>> TargetPlatformSnapshotDescriptors;
	for (const FZenSnapshotDescriptor& SnapshotDescriptor : LatestSnapshotDescriptors)
	{
		ITargetPlatform* TargetPlatform = TargetPlatformManager.FindTargetPlatform(SnapshotDescriptor.GetTargetPlatform());
		if (TargetPlatform)
		{
			TargetPlatformSnapshotDescriptors.FindOrAdd(TargetPlatform).Add(&SnapshotDescriptor);
		}
	}

	for (auto It = TargetPlatformSnapshotDescriptors.CreateConstIterator(); It; ++It)
	{
		const ITargetPlatform* TargetPlatform = It.Key();
		const TArray<const FZenSnapshotDescriptor*>& SnapshotDescriptors = It.Value();

		const FText TargetPlatformLabel = TargetPlatform->DisplayName();
		const FSlateIcon TargetPlatformIcon(FAppStyle::GetAppStyleSetName(), TargetPlatform->GetPlatformInfo().GetIconStyleName(EPlatformIconSize::Normal));

		if (SnapshotDescriptors.Num() == 1)
		{
			Section.AddMenuEntry(
				NAME_None, TargetPlatformLabel, FText::GetEmpty(), TargetPlatformIcon,
				FUIAction(
					FExecuteAction::CreateRaw(this, &ThisClass::SyncSnapshot, SnapshotDescriptors[0]),
					FCanExecuteAction::CreateRaw(this, &ThisClass::CanSyncSnapshot, SnapshotDescriptors[0])
				)
			);
		}
		else
		{
			Section.AddSubMenu(
				NAME_None, TargetPlatformLabel, FText::GetEmpty(),
				FNewToolMenuDelegate::CreateLambda([this, SnapshotDescriptors](UToolMenu* Menu)
				{
					FToolMenuSection& Section = Menu->AddSection(NAME_None);
					for (const FZenSnapshotDescriptor* SnapshotDescriptor : SnapshotDescriptors)
					{
						Section.AddMenuEntry(
							NAME_None, FText::FromString(SnapshotDescriptor->GetName()), FText::GetEmpty(),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateRaw(this, &ThisClass::SyncSnapshot, SnapshotDescriptor),
								FCanExecuteAction::CreateRaw(this, &ThisClass::CanSyncSnapshot, SnapshotDescriptor)
							)
						);
					}
				}),
				false, TargetPlatformIcon
			);
		}
	}
}

bool FZenSnapshotSyncToolbar::CanSyncSnapshot(const FZenSnapshotDescriptor* SnapshotDescriptor) const
{
	return SnapshotDescriptor && !SnapshotSyncTasks.Contains(SnapshotDescriptor->GetTargetPlatform());
}

void FZenSnapshotSyncToolbar::SyncSnapshot(const FZenSnapshotDescriptor* SnapshotDescriptor)
{
	if (!SnapshotDescriptor)
	{
		return;
	}

	FZenSnapshotSyncHandle Handle = SnapshotSyncModule->RequestSnapshotSync(*SnapshotDescriptor);
	if (!Handle.IsValid())
	{
		return;
	}

	FAsyncTaskNotificationConfig TaskNotificationConfig;
	TaskNotificationConfig.TitleText = FText::Format(LOCTEXT("SnapshotSyncTaskTitle", "Syncing snapshot '{0}'"), FText::FromString(SnapshotDescriptor->GetName()));
	TaskNotificationConfig.bKeepOpenOnFailure = true;
	TaskNotificationConfig.bCanCancel = true;

	FZenSnapshotSyncTask& Task = SnapshotSyncTasks.Add(SnapshotDescriptor->GetTargetPlatform());
	Task.Handle = MoveTemp(Handle);
	Task.Notification = MakeUnique<FAsyncTaskNotification>(TaskNotificationConfig);

	if (!SnapshotSyncTickHandle.IsValid())
	{
		SnapshotSyncTickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &ThisClass::TickSnapshotSyncTasks), 1.0f);
	}
}

bool FZenSnapshotSyncToolbar::TickSnapshotSyncTasks(float DeltaTime)
{
	for (auto It = SnapshotSyncTasks.CreateIterator(); It; ++It)
	{
		FZenSnapshotSyncTask& Task = It.Value();

		bool bRemoveTask = !SnapshotSyncModule->QuerySnapshotSyncStatus(Task.Handle);

		if (Task.Notification.IsValid())
		{
			if (Task.Notification->GetPromptAction() == EAsyncTaskNotificationPromptAction::Cancel)
			{
				if (SnapshotSyncModule->CancelSnapshotSync(Task.Handle))
				{
					Task.Notification->SetKeepOpenOnFailure(false);
					bRemoveTask = true;
				}
			}
			else
			{
				Task.Notification->SetProgressText(FText::FromString(Task.Handle.GetState()));
			}
		}

		if (bRemoveTask)
		{
			if (Task.Notification.IsValid())
			{
				Task.Notification->SetProgressText(Task.Handle.IsError() ? FText::FromString(Task.Handle.GetErrorMessage()) : FText::GetEmpty());
				Task.Notification->SetComplete(Task.Handle.IsComplete());
				Task.Notification.Reset();
			}

			It.RemoveCurrent();
		}
	}

	return !SnapshotSyncTasks.IsEmpty();
}

void FZenSnapshotSyncToolbar::CancelSnapshotSyncTasks()
{
	for (auto It = SnapshotSyncTasks.CreateIterator(); It; ++It)
	{
		FZenSnapshotSyncTask& Task = It.Value();

		SnapshotSyncModule->CancelSnapshotSync(Task.Handle);

		if (Task.Notification.IsValid())
		{
			Task.Notification->SetKeepOpenOnFailure(false);
			Task.Notification->SetComplete(false);
			Task.Notification.Reset();
		}
	}

	SnapshotSyncTasks.Reset();
	SnapshotSyncTickHandle.Reset();
}

#undef LOCTEXT_NAMESPACE
