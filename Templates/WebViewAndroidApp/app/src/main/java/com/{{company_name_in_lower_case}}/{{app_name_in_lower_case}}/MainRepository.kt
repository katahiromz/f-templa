package com.{{company_name_in_lower_case}}.{{app_name_in_lower_case}}

import android.content.Context
import android.content.SharedPreferences
import org.json.JSONArray

class MainRepository {

    companion object {

        private const val MainPrefFileKey = "MAIN_PREF_FILE"
        private const val MessageListKey = "MESSAGE_LIST"

        private fun getPrefs(context: Context): SharedPreferences {
            return context.getSharedPreferences(MainPrefFileKey, Context.MODE_PRIVATE)
        }
   }
}