package com.todolist.server.auth;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.util.HexFormat;

/** token 生成与 SHA-256 哈希（服务端只存哈希，与客户端契约一致）。 */
public final class TokenUtil {
    private static final SecureRandom RANDOM = new SecureRandom();
    private static final int TOKEN_BYTES = 24; // 48 hex 字符

    private TokenUtil() {
    }

    /** 生成随机设备 token（仅下发一次）。 */
    public static String randomToken() {
        byte[] b = new byte[TOKEN_BYTES];
        RANDOM.nextBytes(b);
        return HexFormat.of().formatHex(b);
    }

    /** SHA-256 hex 摘要，用于存储/比对 token。 */
    public static String sha256Hex(String value) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            return HexFormat.of().formatHex(md.digest(value.getBytes(StandardCharsets.UTF_8)));
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("SHA-256 不可用", e);
        }
    }
}
