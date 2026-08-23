#include <nall/addons/addons.hpp>

auto AddonsSettings::construct() -> void {
  setCollapsible();
  setVisible(false);

  addonsLabel.setText("Addons").setFont(Font().setBold());
  addonsList.setHeadered();
  addonsList.setBatchable();
  addonsList.onChange([&] { eventChange(); });
  addonsList.onActivate([&](auto) { eventToggle(); });
  enableButton.setText("Enable").onActivate([&] { eventEnable(); });
  disableButton.setText("Disable").onActivate([&] { eventDisable(); });
  refreshButton.setText("Refresh").onActivate([&] { eventRefresh(); });

  refresh();
}

auto AddonsSettings::refresh() -> void {
  addonsList.reset();
  addonsList.append(TableViewColumn().setText("Name"));
  addonsList.append(TableViewColumn().setText("Status"));

  for (auto addon : nall::addons::addonsManager.addons) {
    TableViewItem item{&addonsList};
    item.append(TableViewCell().setText(addon.name));
    item.append(TableViewCell().setText(addon.enabled ? "Enabled" : "Disabled"));
  }

  addonsList.resizeColumns();
  eventChange();
}

auto AddonsSettings::eventChange() -> void {
  auto batched = addonsList.batched();
  enableButton.setEnabled(batched.size() >= 1);
  disableButton.setEnabled(batched.size() >= 1);
}

auto AddonsSettings::eventToggle() -> void {
  auto batched = addonsList.batched();
  if(batched.size() != 1) return;
  auto& item = batched[0];
  auto name = item.cell(0).text();
  for (auto& addon : nall::addons::addonsManager.addons) {
    if (addon.name == name) {
      if (addon.enabled) {
        nall::addons::addonsManager.disable(addon);
      } else {
        nall::addons::addonsManager.enable(addon);
      }
    }
  }

  refresh();
}

auto AddonsSettings::eventEnable() -> void {
  for(auto& item : addonsList.batched()) {
    auto name = item.cell(0).text();
    for (auto& addon : nall::addons::addonsManager.addons) {
      if (addon.name == name) {
          nall::addons::addonsManager.enable(addon);
      }
    }
  }

  refresh();
}

auto AddonsSettings::eventDisable() -> void {
  for(auto& item : addonsList.batched()) {
    auto name = item.cell(0).text();
    for (auto& addon : nall::addons::addonsManager.addons) {
      if (addon.name == name) {
          nall::addons::addonsManager.disable(addon);
      }
    }
  }

  refresh();
}

auto AddonsSettings::eventRefresh() -> void {
  nall::addons::addonsManager.refreshAddons();

  refresh();
}
