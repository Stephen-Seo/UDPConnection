use cmake::Config;
use bindgen;

use std::env;
use std::path::PathBuf;

fn main() {
    let mut config = Config::new("../c_impl");
    let mut dst: PathBuf;
    if env::var("PROFILE").unwrap().eq("release") {
        config.define("CMAKE_BUILD_TYPE", "Release");
        dst = config.build();
        dst.push("lib");
    } else {
        config.define("CMAKE_BUILD_TYPE", "Debug");
        dst = config.build();
        dst.push("build");
    }

    println!("cargo:rustc-link-search=native={}", dst.display());
    println!("cargo:rustc-link-lib=static=UDPConnection");

    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .whitelist_type("UDPC_callback_connected")
        .whitelist_type("UDPC_callback_disconnected")
        .whitelist_type("UDPC_callback_received")
        .whitelist_type("UDPC_Context")
        .whitelist_function("UDPC_init")
        .whitelist_function("UDPC_init_threaded_update")
        .whitelist_function("UDPC_destroy")
        .whitelist_function("UDPC_set_callback_connected")
        .whitelist_function("UDPC_set_callback_disconnected")
        .whitelist_function("UDPC_set_callback_received")
        .whitelist_function("UDPC_check_events")
        .whitelist_function("UDPC_client_initiate_connection")
        .whitelist_function("UDPC_queue_send")
        .whitelist_function("UDPC_get_queue_send_available")
        .whitelist_function("UDPC_get_accept_new_connections")
        .whitelist_function("UDPC_set_accept_new_connections")
        .whitelist_function("UDPC_drop_connection")
        .whitelist_function("UDPC_get_protocol_id")
        .whitelist_function("UDPC_set_protocol_id")
        .whitelist_function("UDPC_get_error")
        .whitelist_function("UDPC_get_error_str")
        .whitelist_function("UDPC_set_logging_type")
        .whitelist_function("UDPC_update")
        .whitelist_function("UDPC_strtoa")
        .opaque_type("UDPC_Context")
        .opaque_type("UDPC_Deque")
        .opaque_type("UDPC_HashMap")
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
