package com.todolist.mobile

import android.app.Activity
import android.os.Bundle
import android.widget.TextView

/**
 * M4 骨架：核心（MergePolicy/SyncEngine/TodoStore）已可 JVM 测试；
 * Compose/Room/Retrofit 真机 UI 于下一阶段接入。
 */
class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val tv = TextView(this)
        tv.text = "ToDoList Mobile (M4 skeleton)\n核心已就绪，真机 UI 待接入"
        tv.textSize = 18f
        setContentView(tv)
    }
}
