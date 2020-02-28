extern crate ethereum_bn128;
extern crate rustc_hex;

use rustc_hex::FromHex;

#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ecpairing() {
    // should pass (multi-point example taken from ethereum test case 'pairingTest')
    let pairing_input = FromHex::from_hex("2eca0c7238bf16e83e7a1e6c5d49540685ff51380f309842a98561558019fc0203d3260361bb8451de5ff5ecd17f010ff22f5c31cdf184e9020b06fa5997db841213d2149b006137fcfb23036606f848d638d576a120ca981b5b1a5f9300b3ee2276cf730cf493cd95d64677bbb75fc42db72513a4c1e387b476d056f80aa75f21ee6226d31426322afcda621464d0611d226783262e21bb3bc86b537e986237096df1f82dff337dd5972e32a8ad43e28a78a96a823ef1cd4debe12b6552ea5f06967a1237ebfeca9aaae0d6d0bab8e28c198c5a339ef8a2407e31cdac516db922160fa257a5fd5b280642ff47b65eca77e626cb685c84fa6d3b6882a283ddd1198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c21800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa").unwrap();
    let mut output = [0u8; 32];
    let mut output_expected = [0u8; 32];
    output_expected[31] = 1u8;
    ethereum_bn128::bn128_pairing(&pairing_input, &mut output).expect("pairing check failed");
    assert!(
        output_expected == output,
        "pairing check did not evaluate to 1"
    );
}
