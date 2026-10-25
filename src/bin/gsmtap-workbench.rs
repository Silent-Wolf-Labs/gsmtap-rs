use std::process::ExitCode;

#[tokio::main]
async fn main() -> ExitCode {
    match gsmtap_rs::run_workbench().await {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("gsmtap-workbench: {error}");
            ExitCode::FAILURE
        }
    }
}
