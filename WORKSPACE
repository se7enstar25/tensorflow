# Uncomment and update the paths in these entries to build the Android demo.
#android_sdk_repository(
#    name = "androidsdk",
#    api_level = 23,
#    build_tools_version = "23.0.1",
#    # Replace with path to Android SDK on your system
#    path = "<PATH_TO_SDK>",
#)
#
#android_ndk_repository(
#    name="androidndk",
#    path="<PATH_TO_NDK>",
#    api_level=21)

new_http_archive(
  name = "gmock_archive",
  url = "https://googlemock.googlecode.com/files/gmock-1.7.0.zip",
  sha256 = "26fcbb5925b74ad5fc8c26b0495dfc96353f4d553492eb97e85a8a6d2f43095b",
  build_file = "google/protobuf/gmock.BUILD",
)

new_http_archive(
  name = "eigen_archive",
  url = "https://bitbucket.org/eigen/eigen/get/b455544.tar.gz",
  sha256 = "d388d3fcb7e2ed5e4ec466f320b472b8803e556f46fab5e085ae70a90f7efa05",
  build_file = "eigen.BUILD",
)

bind(
  name = "gtest",
  actual = "@gmock_archive//:gtest",
)

bind(
  name = "gtest_main",
  actual = "@gmock_archive//:gtest_main",
)

git_repository(
  name = "re2",
  remote = "https://github.com/google/re2.git",
  commit = "791beff",
)

new_http_archive(
  name = "jpeg_archive",
  url = "http://www.ijg.org/files/jpegsrc.v9a.tar.gz",
  sha256 = "3a753ea48d917945dd54a2d97de388aa06ca2eb1066cbfdc6652036349fe05a7",
  build_file = "jpeg.BUILD",
)

new_http_archive(
  name = "png_archive",
  url = "https://storage.googleapis.com/libpng-public-archive/libpng-1.2.53.tar.gz",
  sha256 = "e05c9056d7f323088fd7824d8c6acc03a4a758c4b4916715924edc5dd3223a72",
  build_file = "png.BUILD",
)

new_http_archive(
  name = "six_archive",
  url = "https://pypi.python.org/packages/source/s/six/six-1.10.0.tar.gz#md5=34eed507548117b2ab523ab14b2f8b55",
  sha256 = "105f8d68616f8248e24bf0e9372ef04d3cc10104f1980f54d57b2ce73a5ad56a",
  build_file = "six.BUILD",
)

bind(
  name = "six",
  actual = "@six_archive//:six",
)

# TENSORBOARD_BOWER_AUTOGENERATED_BELOW_THIS_LINE_DO_NOT_EDIT

new_git_repository(
  name = "iron_form_element_behavior",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-form-element-behavior.git",
  tag = "v1.0.6",
)

new_git_repository(
  name = "iron_menu_behavior",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-menu-behavior.git",
  tag = "v1.0.7",
)

new_git_repository(
  name = "paper_toggle_button",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-toggle-button.git",
  tag = "v1.0.11",
)

new_git_repository(
  name = "iron_a11y_keys_behavior",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-a11y-keys-behavior.git",
  tag = "v1.1.1",
)

new_git_repository(
  name = "iron_range_behavior",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-range-behavior.git",
  tag = "v1.0.4",
)

new_git_repository(
  name = "paper_radio_group",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-radio-group.git",
  tag = "v1.0.6",
)

new_git_repository(
  name = "chai",
  build_file = "bower.BUILD",
  remote = "https://github.com/chaijs/chai.git",
  tag = "2.3.0",
)

new_git_repository(
  name = "iron_fit_behavior",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-fit-behavior.git",
  tag = "v1.0.5",
)

new_git_repository(
  name = "paper_menu",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-menu.git",
  tag = "v1.1.1",
)

new_git_repository(
  name = "iron_behaviors",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-behaviors.git",
  tag = "v1.0.10",
)

new_git_repository(
  name = "hydrolysis",
  build_file = "bower.BUILD",
  remote = "https://github.com/Polymer/hydrolysis.git",
  tag = "v1.22.0",
)

new_git_repository(
  name = "graphlib",
  build_file = "bower.BUILD",
  remote = "https://github.com/cpettitt/graphlib.git",
  tag = "v1.0.7",
)

new_git_repository(
  name = "webcomponentsjs",
  build_file = "bower.BUILD",
  remote = "https://github.com/Polymer/webcomponentsjs.git",
  tag = "v0.7.20",
)

new_git_repository(
  name = "accessibility_developer_tools",
  build_file = "bower.BUILD",
  remote = "https://github.com/GoogleChrome/accessibility-developer-tools.git",
  tag = "v2.10.0",
)

new_git_repository(
  name = "paper_styles",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-styles.git",
  tag = "v1.0.12",
)

new_git_repository(
  name = "paper_item",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-item.git",
  tag = "v1.0.5",
)

new_git_repository(
  name = "paper_toolbar",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-toolbar.git",
  tag = "v1.0.4",
)

new_git_repository(
  name = "iron_overlay_behavior",
  build_file = "bower.BUILD",
  remote = "https://github.com/polymerelements/iron-overlay-behavior.git",
  tag = "v1.2.0",
)

new_git_repository(
  name = "paper_input",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-input.git",
  tag = "v1.0.16",
)

new_git_repository(
  name = "iron_component_page",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-component-page.git",
  tag = "v1.1.4",
)

new_git_repository(
  name = "iron_icon",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-icon.git",
  tag = "v1.0.7",
)

new_git_repository(
  name = "iron_selector",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-selector.git",
  tag = "v1.0.7",
)

new_git_repository(
  name = "paper_behaviors",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-behaviors.git",
  tag = "v1.0.9",
)

new_git_repository(
  name = "iron_list",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-list.git",
  tag = "v1.1.5",
)

new_git_repository(
  name = "dagre",
  build_file = "bower.BUILD",
  remote = "https://github.com/cpettitt/dagre.git",
  tag = "v0.7.4",
)

new_git_repository(
  name = "paper_slider",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-slider.git",
  tag = "v1.0.7",
)

new_git_repository(
  name = "iron_input",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-input.git",
  tag = "1.0.8",
)

new_git_repository(
  name = "iron_resizable_behavior",
  build_file = "bower.BUILD",
  remote = "https://github.com/polymerelements/iron-resizable-behavior.git",
  tag = "v1.0.2",
)

new_git_repository(
  name = "sinonjs",
  build_file = "bower.BUILD",
  remote = "https://github.com/blittle/sinon.js.git",
  tag = "v1.17.1",
)

new_git_repository(
  name = "async",
  build_file = "bower.BUILD",
  remote = "https://github.com/caolan/async.git",
  tag = "v1.5.2",
)

new_git_repository(
  name = "mocha",
  build_file = "bower.BUILD",
  remote = "https://github.com/mochajs/mocha.git",
  tag = "v2.4.5",
)

new_git_repository(
  name = "test_fixture",
  build_file = "bower.BUILD",
  remote = "https://github.com/polymerelements/test-fixture.git",
  tag = "v1.1.0",
)

new_git_repository(
  name = "iron_dropdown",
  build_file = "bower.BUILD",
  remote = "https://github.com/polymerelements/iron-dropdown.git",
  tag = "v1.1.0",
)

new_git_repository(
  name = "paper_tabs",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-tabs.git",
  tag = "v1.0.10",
)

new_git_repository(
  name = "paper_checkbox",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-checkbox.git",
  tag = "v1.0.13",
)

new_git_repository(
  name = "web_component_tester",
  build_file = "bower.BUILD",
  remote = "https://github.com/Polymer/web-component-tester.git",
  tag = "v4.1.0",
)

new_git_repository(
  name = "paper_menu_button",
  build_file = "bower.BUILD",
  remote = "https://github.com/polymerelements/paper-menu-button.git",
  tag = "v1.0.4",
)

new_git_repository(
  name = "plottable",
  build_file = "bower.BUILD",
  remote = "https://github.com/palantir/plottable.git",
  tag = "v1.16.2",
)

new_git_repository(
  name = "iron_collapse",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-collapse.git",
  tag = "v1.0.4",
)

new_git_repository(
  name = "paper_ripple",
  build_file = "bower.BUILD",
  remote = "https://github.com/polymerelements/paper-ripple.git",
  tag = "v1.0.5",
)

new_git_repository(
  name = "iron_validatable_behavior",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-validatable-behavior.git",
  tag = "v1.0.5",
)

new_git_repository(
  name = "paper_material",
  build_file = "bower.BUILD",
  remote = "https://github.com/polymerelements/paper-material.git",
  tag = "v1.0.6",
)

new_git_repository(
  name = "paper_progress",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-progress.git",
  tag = "v1.0.7",
)

new_git_repository(
  name = "prism",
  build_file = "bower.BUILD",
  remote = "https://github.com/LeaVerou/prism.git",
  tag = "v1.3.0",
)

new_git_repository(
  name = "marked_element",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/marked-element.git",
  tag = "v1.1.2",
)

new_git_repository(
  name = "d3",
  build_file = "bower.BUILD",
  remote = "https://github.com/mbostock/d3.git",
  tag = "v3.5.6",
)

new_git_repository(
  name = "neon_animation",
  build_file = "bower.BUILD",
  remote = "https://github.com/polymerelements/neon-animation.git",
  tag = "v1.1.0",
)

new_git_repository(
  name = "iron_icons",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-icons.git",
  tag = "v1.1.2",
)

new_git_repository(
  name = "prism_element",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/prism-element.git",
  tag = "v1.0.3",
)

new_git_repository(
  name = "iron_ajax",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-ajax.git",
  tag = "v1.0.7",
)

new_git_repository(
  name = "paper_icon_button",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-icon-button.git",
  tag = "v1.0.5",
)

new_git_repository(
  name = "sinon_chai",
  build_file = "bower.BUILD",
  remote = "https://github.com/domenic/sinon-chai.git",
  tag = "2.8.0",
)

new_git_repository(
  name = "iron_doc_viewer",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-doc-viewer.git",
  tag = "v1.0.12",
)

new_git_repository(
  name = "svg_typewriter",
  build_file = "bower.BUILD",
  remote = "https://github.com/palantir/svg-typewriter.git",
  tag = "v0.3.0",
)

new_git_repository(
  name = "es6_promise",
  build_file = "bower.BUILD",
  remote = "https://github.com/components/es6-promise.git",
  tag = "v3.0.2",
)

new_git_repository(
  name = "marked",
  build_file = "bower.BUILD",
  remote = "https://github.com/chjj/marked.git",
  tag = "v0.3.5",
)

new_git_repository(
  name = "paper_button",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-button.git",
  tag = "v1.0.8",
)

new_git_repository(
  name = "iron_meta",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-meta.git",
  tag = "v1.1.1",
)

new_git_repository(
  name = "iron_checked_element_behavior",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-checked-element-behavior.git",
  tag = "v1.0.4",
)

new_git_repository(
  name = "paper_radio_button",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-radio-button.git",
  tag = "v1.0.10",
)

new_git_repository(
  name = "web_animations_js",
  build_file = "bower.BUILD",
  remote = "https://github.com/web-animations/web-animations-js.git",
  tag = "2.1.3",
)

new_git_repository(
  name = "iron_flex_layout",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-flex-layout.git",
  tag = "v1.2.2",
)

new_git_repository(
  name = "iron_autogrow_textarea",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/iron-autogrow-textarea.git",
  tag = "v1.0.10",
)

new_git_repository(
  name = "lodash",
  build_file = "bower.BUILD",
  remote = "https://github.com/lodash/lodash.git",
  tag = "3.10.1",
)

new_git_repository(
  name = "promise_polyfill",
  build_file = "bower.BUILD",
  remote = "https://github.com/polymerlabs/promise-polyfill.git",
  tag = "v1.0.0",
)

new_git_repository(
  name = "stacky",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerLabs/stacky.git",
  tag = "v1.3.2",
)

new_git_repository(
  name = "font_roboto",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/font-roboto.git",
  tag = "v1.0.1",
)

new_git_repository(
  name = "paper_dropdown_menu",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-dropdown-menu.git",
  tag = "v1.0.5",
)

new_git_repository(
  name = "iron_iconset_svg",
  build_file = "bower.BUILD",
  remote = "https://github.com/polymerelements/iron-iconset-svg.git",
  tag = "v1.0.9",
)

new_git_repository(
  name = "polymer",
  build_file = "bower.BUILD",
  remote = "https://github.com/Polymer/polymer.git",
  tag = "v1.1.5",
)

new_git_repository(
  name = "paper_header_panel",
  build_file = "bower.BUILD",
  remote = "https://github.com/PolymerElements/paper-header-panel.git",
  tag = "v1.0.5",
)
