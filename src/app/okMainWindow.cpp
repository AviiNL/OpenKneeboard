#include "okMainWindow.h"

#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/wupdlock.h>

#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/dprint.h"
#include "Settings.h"
#include "okDirectInputController.h"
#include "okEvents.h"
#include "okGameEventMailslotThread.h"
#include "okGamesList.h"
#include "okOpenVRThread.h"
#include "okSHMRenderer.h"
#include "okTab.h"
#include "okTabsList.h"

using namespace OpenKneeboard;

class okMainWindow::Impl {
 public:
  std::vector<okConfigurableComponent*> Configurables;
  std::vector<okTab*> TabUIs;
  wxNotebook* Notebook = nullptr;
  okTabsList* TabsList = nullptr;
  int CurrentTab = -1;
  Settings Settings = ::Settings::Load();

  std::unique_ptr<okSHMRenderer> SHMRenderer;
};

okMainWindow::okMainWindow()
  : wxFrame(nullptr, wxID_ANY, "OpenKneeboard"), p(std::make_unique<Impl>()) {
  (new okOpenVRThread())->Run();
  (new okGameEventMailslotThread(this))->Run();
  p->SHMRenderer = std::make_unique<okSHMRenderer>();

  this->Bind(okEVT_GAME_EVENT, &okMainWindow::OnGameEvent, this);
  auto menuBar = new wxMenuBar();
  {
    auto fileMenu = new wxMenu();
    menuBar->Append(fileMenu, _("&File"));

    fileMenu->Append(wxID_EXIT, _("E&xit"));
    Bind(wxEVT_MENU, &okMainWindow::OnExit, this, wxID_EXIT);
  }
  {
    auto editMenu = new wxMenu();
    menuBar->Append(editMenu, _("&Edit"));

    auto settingsId = wxNewId();
    editMenu->Append(settingsId, _("&Settings..."));
    Bind(wxEVT_MENU, &okMainWindow::OnShowSettings, this, settingsId);
  }
  SetMenuBar(menuBar);

  p->Notebook = new wxNotebook(this, wxID_ANY);
  p->Notebook->Bind(
    wxEVT_BOOKCTRL_PAGE_CHANGED, &okMainWindow::OnTabChanged, this);

  {
    auto tabs = new okTabsList(p->Settings.Tabs);
    p->TabsList = tabs;
    p->Configurables.push_back(tabs);
    UpdateTabs();
    tabs->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->Settings.Tabs = tabs->GetSettings();
      p->Settings.Save();
      this->UpdateTabs();
    });
  }

  auto sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(p->Notebook, 1, wxEXPAND);
  this->SetSizerAndFit(sizer);

  UpdateSHM();

  {
    auto gl = new okGamesList(p->Settings.Games);
    p->Configurables.push_back(gl);

    gl->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->Settings.Games = gl->GetSettings();
      p->Settings.Save();
    });
  }

  {
    auto dipc = new okDirectInputController(p->Settings.DirectInput);
    p->Configurables.push_back(dipc);

    dipc->Bind(okEVT_PREVIOUS_TAB, &okMainWindow::OnPreviousTab, this);
    dipc->Bind(okEVT_NEXT_TAB, &okMainWindow::OnNextTab, this);
    dipc->Bind(okEVT_PREVIOUS_PAGE, &okMainWindow::OnPreviousPage, this);
    dipc->Bind(okEVT_NEXT_PAGE, &okMainWindow::OnNextPage, this);

    dipc->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->Settings.DirectInput = dipc->GetSettings();
      p->Settings.Save();
    });
  }
}

okMainWindow::~okMainWindow() {
}

void okMainWindow::OnTabChanged(wxBookCtrlEvent& ev) {
  const auto tab = ev.GetSelection();
  if (tab == wxNOT_FOUND) {
    return;
  }
  p->CurrentTab = tab;
  UpdateSHM();
}

void okMainWindow::OnGameEvent(wxThreadEvent& ev) {
  const auto ge = ev.GetPayload<GameEvent>();
  dprintf("GameEvent: '{}' = '{}'", ge.Name, ge.Value);
  for (auto tab: p->TabUIs) {
    tab->GetTab()->OnGameEvent(ge);
  }
}

void okMainWindow::UpdateSHM() {
  std::shared_ptr<Tab> tab;
  unsigned int pageIndex = 0;
  if (p->CurrentTab >= 0 && p->CurrentTab < p->TabUIs.size()) {
    auto tabUI = p->TabUIs.at(p->CurrentTab);
    tab = tabUI->GetTab();
    pageIndex = tabUI->GetPageIndex();
  }
  p->SHMRenderer->Render(tab, pageIndex);
}

void okMainWindow::OnExit(wxCommandEvent& ev) {
  Close(true);
}

void okMainWindow::OnShowSettings(wxCommandEvent& ev) {
  auto w = new wxFrame(this, wxID_ANY, _("Settings"));
  auto s = new wxBoxSizer(wxVERTICAL);

  auto nb = new wxNotebook(w, wxID_ANY);
  s->Add(nb, 1, wxEXPAND);

  for (auto& component: p->Configurables) {
    auto p = new wxPanel(nb, wxID_ANY);
    auto ui = component->GetSettingsUI(p);
    if (!ui) {
      continue;
    }

    auto ps = new wxBoxSizer(wxVERTICAL);
    ps->Add(ui, 1, wxEXPAND, 5);
    p->SetSizerAndFit(ps);

    nb->AddPage(p, ui->GetLabel());
  }

  w->SetSizerAndFit(s);
  w->Show(true);
}

void okMainWindow::OnPreviousTab(wxCommandEvent& ev) {
  p->Notebook->AdvanceSelection(false);
}

void okMainWindow::OnNextTab(wxCommandEvent& ev) {
  p->Notebook->AdvanceSelection(true);
}

void okMainWindow::OnPreviousPage(wxCommandEvent& ev) {
  p->TabUIs[p->CurrentTab]->PreviousPage();
}

void okMainWindow::OnNextPage(wxCommandEvent& ev) {
  p->TabUIs[p->CurrentTab]->NextPage();
}

void okMainWindow::UpdateTabs() {
  auto tabs = p->TabsList->GetTabs();
  wxWindowUpdateLocker noUpdates(p->Notebook);

  auto selected
    = p->CurrentTab >= 0 ? p->TabUIs[p->CurrentTab]->GetTab() : nullptr;
  p->CurrentTab = tabs.empty() ? -1 : 0;
  p->TabUIs.clear();
  p->Notebook->DeleteAllPages();

  for (auto tab: tabs) {
    if (selected == tab) {
      p->CurrentTab = p->Notebook->GetPageCount();
    }

    auto ui = new okTab(p->Notebook, tab);
    p->TabUIs.push_back(ui);

    p->Notebook->AddPage(ui, tab->GetTitle(), selected == tab);
    ui->Bind(okEVT_TAB_PIXELS_CHANGED, [this](auto) { this->UpdateSHM(); });
  }

  UpdateSHM();
}
