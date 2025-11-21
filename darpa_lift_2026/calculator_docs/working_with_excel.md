ok i am making manual entries, how do i reference a variable from another page in the workbook?


Referencing a variable from another sheet in Excel is **simple and reliable** — and *especially easy* now that your workbook uses **Named Ranges**.

You can reference variables in **two ways**:

---

# ✅ **BEST METHOD (Recommended): Use Named Ranges**

Because each variable in your workbook is assigned a **Named Range**, you can reference it from *any sheet* by just typing its name:

### Example:

If you have a variable named:
**`Battery_Voltage`**

Then in ANY cell on ANY sheet you can write:

```
=Battery_Voltage
```

Excel will automatically find the correct sheet and cell.

### Why this is best:

* You never need to remember sheet names
* You never break formulas when restructuring sheets
* Much easier to read and debug

---

# ✅ **Standard Excel Method: SheetName + Cell**

If you want to reference the cell directly (not recommended long-term):

```
=SheetName!A1
```

### Example:

If variable **Battery_Voltage** is stored in sheet **Battery** in cell **C3**, then:

```
=Battery!C3
```

If the sheet name contains spaces or special characters **you must wrap it in apostrophes**:

```
='Mission Profile'!C5
```

---

# 🔍 **How to find the Named Range for a variable**

1. Click the cell that contains the value of the variable
2. Look at the **Name Box** (top left of Excel, to the left of the formula bar)

You will see the name assigned, like:

```
Battery_Voltage
Motor_KV
Payload_Mass
Prop_CT
```

Use that exact name in formulas.

---

# 🔧 If a Named Range wasn’t created automatically

You can assign one manually:

1. Select the cell
2. Click in the **Name Box**
3. Type the variable name (no spaces)
4. Press **Enter**

Now it can be referenced globally.

---

# 🎯 Recommended Practice for Your Calculator

As you rebuild or verify formulas, **always reference variables using named ranges**, never raw sheet/cell references.

Example hover power formula might use:

```
=Prop_CP * AirDensity_Actual * (RPM_Required/60)^3 * Prop_Diameter_m^5 / Motor_Efficiency
```

All using named ranges. Much cleaner.

---

If you want, tell me:

> “Make me a clean list of all named ranges in my workbook”

And I can generate that for you.


how to make new variables in an excel spreadsheet?


Creating **new variables** in Excel is actually very easy — Excel calls them **Named Ranges**.
Once created, they behave just like variables in programming: global, reusable, and self-updating.

Below is the clean method you should use in your drone calculator workbook.

---

# ✅ **Method 1 — Create a Variable (Named Range) Directly**

### **Step-by-step**

1. Click the cell where the variable’s value will live
2. Go to the **Name Box** (top left, next to the formula bar)
3. Type the variable name (no spaces, letters/numbers/underscores only)
4. Press **Enter**

### Example:

You want a variable `Motor_Efficiency`.

1. Click the cell (say C12)
2. Click the Name Box
3. Type:

   ```
   Motor_Efficiency
   ```
4. Press **Enter**

Now in ANY cell in the workbook, you can reference it:

```
=Motor_Efficiency
```

---

# ✅ **Method 2 — Create a Variable Through Formulas → Name Manager**

Use if you want more control or want to edit existing variables.

### Steps

1. Go to the **Formulas** tab
2. Click **Name Manager**
3. Click **New**
4. In *Name*: type the variable name
5. In *Refers to*: click the cell where the value is stored
6. Press OK

### Example:

Define `Gravity` → refers to cell `Constants!C4`.

---

# ✅ **Method 3 — Create Variables for a Whole Table**

If you have a table of variables (like your spreadsheet), you can turn each row into a variable:

1. Click on the table
2. Go to **Formulas → Create from Selection**
3. Choose:

   * **Left column** (if variable names are in column A)
4. Click OK

Excel will automatically create named ranges for each variable.

---

# ✔ What Makes a Good Excel Variable Name?

Use:

* Letters, numbers
* Underscores `_`
* No spaces
* Avoid starting with a number
* Avoid operators (+ - / *)

Examples:

```
Battery_Voltage
Motor_MaxRPM
Prop_Diameter_m
AirDensity_Actual
```

Bad:

```
Battery Voltage   (space)
3V_cell           (starts with digit)
Prop-Diameter     (illegal char)
```

---

# 🧪 Testing the Variable Works

After creating the variable, pick any empty cell and type:

```
=<your_variable_name>
```

If Excel auto-completes or displays the correct value, the variable is set up correctly.

---

# 📌 Tip for Your 13-Sheet Drone Workbook

Every time you add a new physical parameter (e.g., a new drag coefficient or battery value):

1. Put the value in the appropriate sheet
2. Give the cell a variable name
3. Use that variable globally in all formulas

Your workbook becomes:

* Modular
* Error-resistant
* Easy to debug
* Easy to scale

---

If you want, I can also show you:

### ✔ How to automatically create variables for **an entire column**

### ✔ How to change/delete unused variables

### ✔ How to inspect all variables in your workbook

### ✔ How to auto-fill formulas using those variables

Just tell me!


