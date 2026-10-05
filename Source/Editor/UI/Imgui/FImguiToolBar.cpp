#include "FImguiToolBar.h"
#include "ThirdParty/Imgui/imgui.h"
#include "Editor/Core/FEditor.h"
#include "FImguiEditorViewportWindow.h"
#include "Runtime/Resource/FResourceLoader.h"
#include <algorithm>
// "표시명\0패턴\0" 이중 널 종료 필요
constexpr wchar_t SceneFilter[] = L"Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
constexpr wchar_t ObjFilter[] = L"Scene Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";

void FImguiToolbar::Process(FEditor& Editor, FImguiConsoleWindow& ConsoleWindow, FImguiControlPanelWindow& ControlPanelWindow, FImguiPropertyWindow& PropertyWindow)
{
    if (Editor.bZenMode)
    {
        return;
    }

    static FString CurrentScenePath;

	if (ImGui::BeginMainMenuBar()) 
    {
        // PIE 중에는 씬 교체와 편집 도구 변경을 막는다.
        ImGui::BeginDisabled(Editor.IsPlaying());

        //씬 저장,로드 기능
        ShowFileBar(CurrentScenePath, Editor);

        //Imgui Window들 소환
        ShowViewBar(Editor, ConsoleWindow);

        ImGui::EndDisabled();

        ShowPlayBar(Editor);

        ImGui::EndMainMenuBar();
	}
}

// 취소하면 false
bool FImguiToolbar::PickSceneFile(FString& OutPath, bool bSave)
{
    wchar_t Buffer[MAX_PATH]{};               

    OPENFILENAMEW Desc{};
    Desc.lStructSize = sizeof(Desc);
    Desc.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
    Desc.lpstrFilter = SceneFilter;
    Desc.lpstrFile = Buffer;
    Desc.nMaxFile = MAX_PATH;
    Desc.lpstrDefExt = L"Scene";
    // OFN_NOCHANGEDIR 없으면 대화상자가 프로세스 현재 디렉터리를 바꿔서
    // 이후 상대 경로 로딩(셰이더/텍스처)이 조용히 깨진다
    Desc.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR
        | (bSave ? OFN_OVERWRITEPROMPT : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST));

    if (!(bSave ? GetSaveFileNameW(&Desc) : GetOpenFileNameW(&Desc)))
        return false;

   OutPath = std::filesystem::path(Buffer).string();
    return true;
}

void FImguiToolbar::ShowFileBar(FString CurrentScenePath, FEditor& Editor)
{


    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New Scene"))
        {
            Editor.NewScene();
            CurrentScenePath.clear();
        }
        if (ImGui::MenuItem("Save Scene"))
        {
            // 경로가 있으면 그대로 덮어쓰고, 없으면 다른 이름으로 저장과 같게 동작
            if (CurrentScenePath.empty())
            {
                FString Path;
                if (PickSceneFile(Path, true))
                {
                    CurrentScenePath = Path;
                    Editor.SaveWorld(Path);
                }
            }
            else
            {
                Editor.SaveWorld(CurrentScenePath);
            }
        }

        if (ImGui::MenuItem("Save scene as..."))
        {
            FString Path;
            if (PickSceneFile(Path, true))
            {
                CurrentScenePath = Path;
                Editor.SaveWorld(Path);
            }
        }

        if (ImGui::MenuItem("Load Scene"))
        {
            FString Path;
            if (PickSceneFile(Path, false))
            {
                CurrentScenePath = Path;
                Editor.LoadWorld(Path);
            }
        }

        if (ImGui::MenuItem("Import Import"))
        {
            FString Path;
            if (PickObjFile(Path))
            {
                FResourceLoader::ImportObj(Path);
            }
        }

        ImGui::EndMenu();
    }

}

void FImguiToolbar::ShowViewBar(FEditor& Editor, FImguiConsoleWindow& ConsoleWindow)
{
    if (ImGui::BeginMenu("View"))
    {
        if (ImGui::BeginMenu("ViewPort"))
        {
            if (ImGui::MenuItem("Single"))
            {
                Editor.SetViewLayout(FEditorState::SplitViewMode::SINGLE);
            }
            if (ImGui::MenuItem("Top | Bottom"))
            {
                Editor.SetViewLayout(FEditorState::SplitViewMode::VERTICAL);

            }
            if (ImGui::MenuItem("Left | Right"))
            {
                Editor.SetViewLayout(FEditorState::SplitViewMode::HORIZONTAL);

            }
            if (ImGui::MenuItem("2 X 2"))
            {
                Editor.SetViewLayout(FEditorState::SplitViewMode::QUAD);

            }

            ImGui::EndMenu();

        }

        ImGui::MenuItem("Hide UI", nullptr, &Editor.bHideUI);
        ImGui::MenuItem("Show Benchmark UI", nullptr, &Editor.bShowBenchmark);

        ImGui::EndMenu();
    }

    auto& Gizmo = Editor.GetGizmo();

    static const char* GizmoModes[4] = { "None", "Translation", "Rotation", "Scale" };
    const int SelectedItem = static_cast<int>(Editor.GetGizmo().Mode);
    if (ImGui::Button(GizmoModes[SelectedItem], { 150.0f, 0.0f }))
    {
        Gizmo.Mode = static_cast<EGizmoMode>((SelectedItem + 1) % 4);
    }
}

bool FImguiToolbar::PickObjFile(FString& OutPath)
{
    wchar_t Buffer[MAX_PATH]{};

    OPENFILENAMEW Desc{};
    Desc.lStructSize = sizeof(Desc);
    Desc.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
    Desc.lpstrFilter = ObjFilter;
    Desc.lpstrFile = Buffer;
    Desc.nMaxFile = MAX_PATH;
    Desc.lpstrDefExt = L"obj";    
    Desc.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&Desc))
    {
        return false;
    }        

    OutPath = std::filesystem::path(Buffer).string();
    return true;
}

void FImguiToolbar::ShowPlayBar(FEditor& Editor)
{
    const ImGuiStyle& Style = ImGui::GetStyle();

    // 모든 라벨 중 가장 긴 글자에 맞춰 버튼 너비를 통일한다.
    const float TextWidth = std::max({
        ImGui::CalcTextSize("Play").x,
        ImGui::CalcTextSize("Pause").x,
        ImGui::CalcTextSize("Resume").x,
        ImGui::CalcTextSize("Stop").x
        });
    const float ButtonWidth = TextWidth + Style.FramePadding.x * 2.0f;

    // 버튼 세 개와 그 사이 간격 두 개로 중앙 배치에 필요한 폭을 계산한다.
    const float Spacing = Style.ItemSpacing.x;
    const float GroupWidth = ButtonWidth * 3.0f + Spacing * 2.0f;
    const float CenterX = (ImGui::GetWindowSize().x - GroupWidth) * 0.5f;

    // 메뉴바 중앙에 배치하되 좁은 창에서는 왼쪽 메뉴와 겹치지 않게 한다.
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), CenterX));

    ImGui::BeginDisabled(Editor.IsPlaying());
    if (ImGui::Button("Play", ImVec2(ButtonWidth, 0.0f)))
        Editor.StartPIE();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!Editor.IsPlaying());

    // 일시정지는 새 World를 만들거나 BeginPlay를 다시 호출하지 않는다.
    const char* PauseLabel = Editor.IsPIEPaused() ? "Resume" : "Pause";
    if (ImGui::Button(PauseLabel, ImVec2(ButtonWidth, 0.0f)))
        Editor.TogglePIEPause();

    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(ButtonWidth, 0.0f)))
        Editor.EndPIE();

    ImGui::EndDisabled();
}